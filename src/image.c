#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <vips/vips.h>

#include "image.h"
#include "renderer.h"
#include "utils.h"

static void image_unloaded_to_loaded(Image *const self, int const w, int const h, unsigned char const channels,
                                     unsigned char *const pixels, SpecificImage const spec)
{
    image_assert_unloaded(self);

    self->width = w;
    self->height = h;
    self->pixels = pixels;
    self->channels = channels;
    self->spec = spec;

    self->state = IMAGE_STATE_LOADED;
}

static void image_loaded_to_uploaded(Image *const self, unsigned int const texture)
{
    image_assert_loaded(self);

    self->texture = texture;
    self->recenter = true;
    self->rerender = true;
    if (self->pixels)
    {
        g_free(self->pixels);
        self->pixels = nullptr;
    }

    self->state = IMAGE_STATE_UPLOADED;
}

static StaticImage static_image_create()
{
    return (StaticImage){};
}

static void static_image_destroy([[maybe_unused]] StaticImage const *const self)
{
}

static AnimatedImage animated_image_create(int const count, int delays[])
{
    return (AnimatedImage){
        .count = count,
        .delays_ms = delays,
    };
}

static void animated_image_destroy(AnimatedImage const *const self)
{
    if (self->delays_ms)
        free(self->delays_ms);
}

static int bsearch_strcasecmp(void const *const s1, void const *const s2)
{
    char const *const key = *(char const *const *const)s1;
    char const *const element = *(char const *const *const)s2;
    return strcasecmp(key, element);
}

static ImageType image_file_get_type(char const *const path)
{
    static char const *animation_extensions[] = {"ani", "avif", "gif", "jxl", "webp"};

    if (!path)
        return false;

    char const *const dot = strrchr(path, '.');
    if (!dot)
        return false;

    char const *const ext = dot + 1;
    if (ext[0] == '\0') // Trailing dot
        return false;

    bool const is_anim =
        bsearch(&ext, animation_extensions, sizeof(animation_extensions) / sizeof(animation_extensions[0]),
                sizeof(char *), bsearch_strcasecmp);

    return is_anim ? IMAGE_TYPE_ANIMATED : IMAGE_TYPE_STATIC;
}

static void image_to_unloaded(Image *const self)
{
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
    switch (self->spec.type)
    {
    case IMAGE_TYPE_STATIC:
        static_image_destroy(&self->spec.stat);
        break;
    case IMAGE_TYPE_ANIMATED:
        animated_image_destroy(&self->spec.anim);
        break;
    }

    self->state = IMAGE_STATE_UNLOADED;
}

static int *get_delays(VipsImage *in, int const frame_count, int *count)
{
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

static VipsImage *vips_image_prepare_internal(char const *const path, ImageType const type)
{
    VipsImage *tmp1;
    switch (type)
    {
    case IMAGE_TYPE_STATIC:
        tmp1 = vips_image_new_from_file(path, nullptr);
        break;
    case IMAGE_TYPE_ANIMATED:
        tmp1 = vips_image_new_from_file(path, "n", -1, nullptr);
        break;
    default:
        panic("Unreachable");
    }
    if (!tmp1)
        goto err;

    VipsImage *tmp2;
    if (vips_colourspace(tmp1, &tmp2, VIPS_INTERPRETATION_sRGB, nullptr) != 0)
        goto err_tmp1;

    VipsImage *img;
    if (vips_cast(tmp2, &img, VIPS_FORMAT_UCHAR, nullptr) != 0)
        goto err_tmp2;

    return img;

err_tmp2:
    g_object_unref(tmp2);
err_tmp1:
    g_object_unref(tmp1);
err:
    fprintf(stderr, "%s", vips_error_buffer());
    return nullptr;
}

static bool vips_image_get_fields_internal(VipsImage *const img, ImageType const type, int *const width,
                                           int *const height, int *const channels, int *const count, int **const delays,
                                           unsigned char **const pixels)
{
    assert(img != nullptr && width != nullptr && height != nullptr && channels != nullptr && count != nullptr &&
           delays != nullptr && pixels != nullptr);

    size_t size;
    *pixels = vips_image_write_to_memory(img, &size);
    if (!*pixels)
        goto err;

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
            goto err_pixels;
        break;
    }

    assert((size_t)*count * (size_t)*width * (size_t)*height * (size_t)*channels == size);
    return true;

err_pixels:
    g_free(*pixels);
err:
    LOG(LOG_ERROR, "Failed to get image fields");
    return false;
}

void image_create(Image *const self, char const *const path)
{
    assert(self != nullptr);

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
    image_assert(self);

    image_to_unloaded(self);

    if (self->path)
        free(self->path);
}

bool image_load(Image *const self)
{
    unsigned long long const start = time_now_ms();

    image_assert_unloaded(self);

    ImageType const type = image_file_get_type(self->path);
    VipsImage *v_img = vips_image_prepare_internal(self->path, type);
    if (!v_img)
        goto err;

    int w, h, channels;
    int count, *delays;
    unsigned char *pixels;
    if (!vips_image_get_fields_internal(v_img, type, &w, &h, &channels, &count, &delays, &pixels))
        goto err_v_img;

    SpecificImage spec = {.type = type};
    switch (spec.type)
    {
    case IMAGE_TYPE_STATIC:
        spec.stat = static_image_create();
        break;
    case IMAGE_TYPE_ANIMATED:
        spec.anim = animated_image_create(count, delays);
        break;
    }

    image_unloaded_to_loaded(self, w, h, channels, pixels, spec);

    g_object_unref(v_img);
    LOG(LOG_INFO, "Loaded image \"%s\" in %llds", path_basename(self->path), time_since_ms(start));
    return true;

err_v_img:
    g_object_unref(v_img);
err:
    image_to_unloaded(self);
    return false;
}

static void *image_load_callback(void *const arg)
{
    if (!image_load(arg))
        panic("Failed to load image");

    glfwPostEmptyEvent();
    return nullptr;
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
    image_assert_loaded(self);

    unsigned long long const start = time_now_ms();

    if (self->state == IMAGE_STATE_UPLOADED)
        return;

    unsigned int const texture =
        texture_create(ren, self->width, self->height, image_frame_count(self), self->channels, self->pixels);
    if (!texture)
        panic("Failed to create texture");

    image_loaded_to_uploaded(self, texture);

    LOG(LOG_INFO, "Uploaded image in %lldms", time_since_ms(start));
}

void image_file_get_size(char const *const path, int *const width, int *const height)
{
    VipsImage *const img = vips_image_new_from_file(path, nullptr);
    if (!img)
        panic("Failed to load image: %s", vips_error_buffer());

    *width = vips_image_get_width(img);
    *height = vips_image_get_page_height(img);
    *width = vips_image_get_width(img);
    *height = vips_image_get_page_height(img);

    g_object_unref(img);
}
