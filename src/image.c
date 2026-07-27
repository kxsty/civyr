#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <vips/vips.h>

#include "image.h"
#include "renderer.h"
#include "utils.h"

static void static_image_create([[maybe_unused]] StaticImage const *const self)
{
}

static void static_image_destroy([[maybe_unused]] StaticImage const *const self)
{
}

static void animated_image_create(AnimatedImage *const self, unsigned const count, int *const delays_ms)
{
    assert(self != nullptr && count > 0 && delays_ms != nullptr);

    *self = (AnimatedImage){
        .count = count,
        .delays_ms = delays_ms,
    };
}

static void animated_image_destroy(AnimatedImage const *const self)
{
    animated_image_assert(self);

    if (self->delays_ms)
        free(self->delays_ms);
}

static void image_unloaded_to_loaded(Image *const self, unsigned const w, unsigned const h,
                                     unsigned char const channels, SpecificImage const spec,
                                     unsigned char *const pixels)
{
    image_assert_unloaded(self);
    assert(pixels != nullptr);

    self->width = w;
    self->height = h;
    self->channels = channels;
    self->spec = spec;
    self->pixels = pixels;

    self->state = IMAGE_STATE_LOADED;
}

static void image_loaded_to_uploaded(Image *const self, unsigned const texture)
{
    image_assert_loaded(self);

    if (self->pixels)
    {
        g_free(self->pixels);
        self->pixels = nullptr;
    }
    self->texture = texture;
    self->recenter = true;
    self->rerender = true;

    self->state = IMAGE_STATE_UPLOADED;
}

static void image_to_unloaded(Image *const self)
{
    assert(self != nullptr);

    switch (self->spec.type)
    {
    case IMAGE_TYPE_STATIC:
        static_image_destroy(&self->spec.stat);
        break;
    case IMAGE_TYPE_ANIMATED:
        animated_image_destroy(&self->spec.anim);
        break;
    }
    if (self->pixels)
    {
        g_free(self->pixels);
        self->pixels = nullptr;
    }
    if (self->texture)
    {
        texture_destroy(self->texture);
        self->texture = 0;
    }

    self->state = IMAGE_STATE_UNLOADED;
}

static int *get_delays(VipsImage *in, unsigned const frame_count, unsigned *const count)
{
    assert(in != nullptr && frame_count > 0 && count != nullptr);

    // Owned by vips
    int *delays_tmp, delay_count;
    if (vips_image_get_array_int(in, "delay", &delays_tmp, &delay_count) != 0)
        return nullptr;

    *count = delay_count > frame_count ? delay_count : frame_count;
    int *const delays = malloc(*count * sizeof(int));
    if (!delays)
        return nullptr;

    if (delay_count == frame_count)
        for (int i = 0; i < *count; i++)
            delays[i] = delays_tmp[i];
    else
        for (int i = 0; i < *count; i++)
            delays[i] = 100;

    return delays;
}

static VipsImage *vips_image_prepare_internal(char const *const path, ImageType *const type)
{
    assert(path != nullptr && *path != '\0' && type != nullptr);

    VipsImage *tmp1 = vips_image_new_from_file(path, nullptr);
    if (!tmp1)
        goto err;

    int const count = vips_image_get_n_pages(tmp1);
    assert(count > 0);
    bool const have_delays = vips_image_get_array_int(tmp1, "delay", nullptr, nullptr) == 0;
    if (count == 1 || have_delays)
    {
        *type = IMAGE_TYPE_STATIC;
    }
    else
    {
        g_object_unref(tmp1);

        tmp1 = vips_image_new_from_file(path, "n", -1, nullptr);
        if (!tmp1)
            goto err;

        *type = IMAGE_TYPE_ANIMATED;
    }

    VipsImage *tmp2;
    if (vips_colourspace(tmp1, &tmp2, VIPS_INTERPRETATION_sRGB, nullptr) != 0)
        goto err_tmp1;

    VipsImage *img;
    if (vips_cast(tmp2, &img, VIPS_FORMAT_UCHAR, nullptr) != 0)
        goto err_tmp2;

    g_object_unref(tmp1);
    g_object_unref(tmp2);
    return img;

err_tmp2:
    g_object_unref(tmp2);
err_tmp1:
    g_object_unref(tmp1);
err:
    fprintf(stderr, "%s", vips_error_buffer());
    vips_error_clear();
    return nullptr;
}

static bool vips_image_get_fields_internal(VipsImage *const img, ImageType const type, unsigned *const width,
                                           unsigned *const height, unsigned char *const channels, unsigned *const count,
                                           int **const delays, unsigned char **const pixels)
{
    assert(img != nullptr && width != nullptr && height != nullptr && channels != nullptr && count != nullptr &&
           delays != nullptr && pixels != nullptr);

    size_t size;
    *pixels = vips_image_write_to_memory(img, &size);
    if (!*pixels)
    {
        LOG(LOG_ERROR, "Failed to load image pixel data");
        return false;
    }

    *width = vips_image_get_width(img);
    *height = vips_image_get_page_height(img);
    *channels = vips_image_get_bands(img);

    switch (type)
    {
    case IMAGE_TYPE_STATIC:
        *count = 1;
        break;
    case IMAGE_TYPE_ANIMATED:
        *count = vips_image_get_n_pages(img);
        *delays = get_delays(img, *count, count);
        if (!*delays)
        {
            g_free(*pixels);
            LOG(LOG_ERROR, "Failed to load image metadata");
            return false;
        }
        break;
    }

    assert((size_t)*count * (size_t)*width * (size_t)*height * (size_t)*channels == size);
    return true;
}

static void *image_load_callback(void *const arg)
{
    if (!image_load(arg))
        panic("Failed to load image");

    glfwPostEmptyEvent();
    return nullptr;
}

void image_create(Image *const self, char const *const path)
{
    assert(self != nullptr && (path == nullptr || path[0] != '\0'));

    char *const path_c = path ? strdup(path) : nullptr;
    if (path && !path_c)
        panic("%s", strerror(errno));

    *self = (Image){
        .path = path_c,
        .state = IMAGE_STATE_UNLOADED,
    };
}

void image_destroy(Image *const self)
{
    assert(self != nullptr);

    image_to_unloaded(self);

    if (self->path)
        free(self->path);
}

bool image_load(Image *const self)
{
#ifndef NDEBUG
    long long const start_ns = time_now_ns();
#endif
    image_assert_unloaded(self);

    unsigned long long const start = time_now_ms();

    ImageType type;
    VipsImage *const v_img = vips_image_prepare_internal(self->path, &type);
    if (!v_img)
        goto err;

    unsigned w, h;
    unsigned char channels;
    unsigned count;
    int *delays;
    unsigned char *pixels;
    if (!vips_image_get_fields_internal(v_img, type, &w, &h, &channels, &count, &delays, &pixels))
        goto err_v_img;

    SpecificImage spec = {.type = type};
    switch (spec.type)
    {
    case IMAGE_TYPE_STATIC:
        static_image_create(&spec.stat);
        break;
    case IMAGE_TYPE_ANIMATED:
        animated_image_create(&spec.anim, count, delays);
        break;
    }

    image_unloaded_to_loaded(self, w, h, channels, spec, pixels);

    g_object_unref(v_img);

#ifndef NDEBUG
    long long const end_ns = time_since_ns(start_ns);
    LOG(LOG_INFO, "Loaded image \"%s\" in %fs", path_basename(self->path), (double)end_ns / NSEC_PER_SEC);
#endif
    return true;

err_v_img:
    g_object_unref(v_img);
err:
    image_to_unloaded(self);
    return false;
}

void image_load_detached(Image *const self)
{
    image_assert_unloaded(self);

    pthread_attr_t attr;
    int res = pthread_attr_init(&attr);
    if (res != 0)
        panic("Failed to initialize thread attribute: %s", strerror(res));

    res = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (res != 0)
        panic("Failed to set detach state thread attribute: %s", strerror(res));

    pthread_t th;
    res = pthread_create(&th, &attr, image_load_callback, self);
    if (res != 0)
    {
        res = pthread_attr_destroy(&attr);
        if (res != 0)
            fprintf(stderr, "Failed to destroy thread attribute: %s", strerror(res));
        panic("Failed to create thread: %s", strerror(res));
    }

    res = pthread_attr_destroy(&attr);
    if (res != 0)
        panic("Failed to destroy thread attribute: %s", strerror(res));
}

void image_upload(Image *const self, Renderer const *ren)
{
#ifndef NDEBUG
    long long const start_ns = time_now_ns();
#endif
    image_assert_loaded(self);
    renderer_assert_initialized(ren);

    unsigned long long const start = time_now_ms();

    if (self->state == IMAGE_STATE_UPLOADED)
        return;

    unsigned int const texture =
        texture_create(ren, self->width, self->height, image_frame_count(self), self->channels, self->pixels);
    if (!texture)
        panic("Failed to create texture");

    image_loaded_to_uploaded(self, texture);

#ifndef NDEBUG
    long long const end_ns = time_since_ns(start_ns);
    LOG(LOG_INFO, "Uploaded image in %fs", (double)end_ns / NSEC_PER_SEC);
#endif
}

void image_file_get_size(char const *const path, unsigned *const width, unsigned *const height)
{
    assert(path != nullptr && *path != '\0' && width != nullptr && height != nullptr);

    VipsImage *const img = vips_image_new_from_file(path, nullptr);
    if (!img)
        panic("Failed to load image: %s", vips_error_buffer());

    *width = vips_image_get_width(img);
    *height = vips_image_get_page_height(img);

    g_object_unref(img);
}
