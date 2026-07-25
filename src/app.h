#ifndef CIVYR_DOMAIN_H
#define CIVYR_DOMAIN_H

#include "image.h"
#include "renderer.h"
#include "window.h"

static unsigned char BG_RGBA[] = {25, 25, 25, 255};

typedef struct App
{
    Renderer ren;
    Image img;
    Window win;
} App;

void app_create(App *self, char const *argv0, char const *image_path);
void app_destroy(App *self);

void app_render_bg(App const *self);
void app_render_image(App *self);
void app_center_image(App *self);
void app_zoom_image(App *self, bool inward);
void app_move_image(App *self, double x_offset, double y_offset);
void app_rotate_image(App *self, bool clockwise);
void app_upload_image(App *self);

#ifdef NDEBUG
#define app_assert(self) (void)(0)
#else
#define app_assert(self)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        window_assert(&(self)->win);                                                                                   \
        image_assert(&(self)->img);                                                                                    \
        renderer_assert(&(self)->ren);                                                                                 \
    } while (0)
#endif

#endif // CIVYR_DOMAIN_H
