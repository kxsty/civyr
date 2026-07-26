#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <log.h>
#include <vips/vips.h>

#include "app.h"

static void calc_window_rect(Window const *const win, int *x, int *y, unsigned *const width, unsigned *const height)
{
    assert(x != nullptr && y != nullptr && width != nullptr && height != nullptr);

    int wx, wy;
    unsigned ww, wh;
    window_workarea(win, &wx, &wy, &ww, &wh);

    unsigned const max_w = (unsigned)(ww * 0.75);
    unsigned const max_h = (unsigned)(wh * 0.75);

    double const scale = fmin((double)max_w / *width, (double)max_h / *height);

    *width = (unsigned)(*width * scale);
    *height = (unsigned)(*height * scale);

    *x = (int)(wx + (ww - *width) / 2);
    *y = (int)(wy + (wh - *height) / 2);
}

static void update_animation(AnimatedImage *const anim)
{
    assert(anim != nullptr);

    long long const now_ms = time_now_ms();

    if (anim->next_frame_ms == 0)
        anim->next_frame_ms = now_ms + anim->delays_ms[0];

    while (now_ms >= anim->next_frame_ms)
    {
        anim->curr_frame = (anim->curr_frame + 1) % anim->count;

        int delay_ms = anim->delays_ms[anim->curr_frame];
        if (delay_ms <= 10)
            delay_ms = 100;

        anim->next_frame_ms += delay_ms;
    }
}

static void extent_rotate(unsigned *const width, unsigned *const height, double const angle)
{
    assert(width != nullptr && height != nullptr);

    switch ((int)angle)
    {
    case 0:
    case 180:
        break;
    case 90:
    case 270: {
        unsigned const tmp = *width;
        *width = *height;
        *height = tmp;
        break;
    }
    default:
        panic("Unsupported angle");
    }
}

void app_create(App *const self, char const *argv0, char const *image_path)
{
    assert(self != nullptr);

    LOG(LOG_TRACE, "Inititalizing vips");
    if (VIPS_INIT(argv0) != 0)
        panic("vips initialization failed: %s", vips_error_buffer());

    vips_cache_set_max(0);

    image_create(&self->img, image_path);

    char const *win_title;
    if (image_path)
    {
        LOG(LOG_TRACE, "Starting image loading");
        image_load_detached(&self->img);

        win_title = path_basename(image_path);
    }
    else
    {
        win_title = "civyr";
    }

    LOG(LOG_TRACE, "Initializing glfw");
    if (!glfwInit())
    {
        char const *err;
        glfwGetError(&err);
        panic("glfw initalization failed: %s", err);
    }

    LOG(LOG_TRACE, "Creating window and renderer");
    if (!window_and_renderer_create(&self->win, &self->ren, win_title))
        abort();

    glfwSetWindowUserPointer(self->win.base, self);
}

void app_destroy(App *const self)
{
    assert(self != nullptr);

    image_destroy(&self->img);
    renderer_destroy(&self->ren);
    window_destroy(&self->win);

    vips_shutdown();
    glfwTerminate();
}

void app_render_bg(App const *self)
{
    app_assert(self);

    renderer_draw_color(&self->ren, BG_RGBA);
    renderer_present(&self->ren);
}

void app_render_image(App *const self)
{
    app_assert(self);
    image_assert_uploaded(&self->img);

    renderer_draw_color(&self->ren, BG_RGBA);

    if (self->img.spec.type == IMAGE_TYPE_ANIMATED)
        update_animation(&self->img.spec.anim);

    renderer_draw_texture(&self->ren, self->img.texture, self->img.width, self->img.height,
                          image_curr_frame(&self->img));

    renderer_present(&self->ren);

    self->img.rerender = false;
}

void app_center_image(App *const self)
{
    app_assert(self);
    image_assert_uploaded(&self->img);

    unsigned width = self->img.width, height = self->img.height;
    extent_rotate(&width, &height, self->ren.camera.angle);

    self->ren.camera.zoom = fmin((double)self->win.width / width, (double)self->win.height / height);

    self->ren.camera.x = 0;
    self->ren.camera.y = 0;
    LOG(LOG_TRACE, "camera:{zoom:%f,x:%f,y:%f,angle:%f}", self->ren.camera.zoom, self->ren.camera.x, self->ren.camera.y,
        self->ren.camera.angle);

    self->img.recenter = false;
}

static void app_window_rename(App const *const self)
{
    app_assert(self);
    image_assert_loaded(&self->img);

    char title[512];
    snprintf(title, sizeof(title), "%s - %i x %i", path_basename(self->img.path), self->img.width, self->img.height);

    window_rename(&self->win, title);
}

void app_upload_image(App *const self)
{
    app_assert(self);
    image_assert_loaded(&self->img);

    int x, y;
    unsigned width = self->img.width, height = self->img.height;
    calc_window_rect(&self->win, &x, &y, &width, &height);

    window_transform(&self->win, x, y, width, height);

    app_window_rename(self);

    image_upload(&self->img, &self->ren);
    image_assert_uploaded(&self->img);
}

void app_zoom_image(App *const self, bool const inward)
{
    app_assert(self);
    image_assert_uploaded(&self->img);

    double const zoom_delta = inward ? 1.25 : 1 / 1.25;
    double zoom = self->ren.camera.zoom * zoom_delta;

    double const scale = fmin((double)self->win.width / self->img.width, (double)self->win.height / self->img.height);
    zoom = fmax(zoom, scale * 0.01);
    zoom = fmin(zoom, scale * 1000);
    if (zoom == self->ren.camera.zoom)
        return;

    double const mouse_math_x = self->win.mouse.x - self->win.width / 2.0;
    double const mouse_math_y = self->win.mouse.y - self->win.height / 2.0;

    double const old_img_x = self->ren.camera.x + mouse_math_x / self->ren.camera.zoom;
    double const old_img_y = -self->ren.camera.y + mouse_math_y / self->ren.camera.zoom;

    self->ren.camera.zoom = zoom;

    self->ren.camera.x = old_img_x - mouse_math_x / self->ren.camera.zoom;
    self->ren.camera.y = -(old_img_y - mouse_math_y / self->ren.camera.zoom);
    LOG(LOG_TRACE, "camera:{x:%f,y:%f}", self->ren.camera.x, self->ren.camera.y);

    self->img.rerender = true;
}

void app_mirror_image(App *self, bool const x, bool const y)
{
    app_assert(self);
    image_assert_uploaded(&self->img);

    if (x)
        self->ren.camera.mirror_x = !self->ren.camera.mirror_x;
    if (y)
        self->ren.camera.mirror_y = !self->ren.camera.mirror_y;

    self->img.rerender = true;
}

void app_move_image(App *const self, double const x_offset, double const y_offset)
{
    app_assert(self);
    image_assert_uploaded(&self->img);

    self->ren.camera.x -= x_offset / self->ren.camera.zoom;
    self->ren.camera.y += y_offset / self->ren.camera.zoom;

    self->img.rerender = true;
}

void app_rotate_image(App *const self, bool const clockwise)
{
    app_assert(self);
    image_assert_uploaded(&self->img);

    int const delta = clockwise ? 90 : -90;
    self->ren.camera.angle = ((int)self->ren.camera.angle + delta + 360) % 360;

    self->img.recenter = true;
    self->img.rerender = true;
}
