#ifndef CIVYR_WINDOW_H
#define CIVYR_WINDOW_H

#include "renderer.h"

typedef struct Mouse
{
    double x;
    double y;
    bool is_dragging;
} Mouse;

typedef struct Window
{
    Mouse mouse;
    GLFWwindow *base;
    unsigned width;
    unsigned height;
} Window;

bool window_and_renderer_create(Window *self, Renderer *ren, char const *title, unsigned width, unsigned height);
void window_destroy(Window const *self);

void window_transform(Window const *self, int x, int y, unsigned width, unsigned height);
void window_workarea(Window const *self, int *x, int *y, unsigned *width, unsigned *height);
void window_toggle_fullscreen(Window const *self);
void window_rename(Window const *self, char const *title);

#ifdef NDEBUG
#define window_assert(self) (void)(0)
#else
#define window_assert(self)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        assert((self)->base != nullptr);                                                                               \
        assert((self)->width > 0);                                                                                     \
        assert((self)->height > 0);                                                                                    \
    } while (0)
#endif

#endif // CIVYR_WINDOW_H
