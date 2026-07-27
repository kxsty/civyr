#ifndef CIVYR_RENDERER_H
#define CIVYR_RENDERER_H

#include <stdlib.h>

#include <GLFW/glfw3.h>

#include "log.h"
#include "window.h"

typedef struct Camera
{
    double zoom;
    double x;
    double y;
    double angle;
    bool mirror_x;
    bool mirror_y;
} Camera;

typedef struct Uniforms
{
    int mvp;
    int depth;
} Uniforms;

typedef struct Shaders
{
    unsigned program;
    Uniforms uniforms;
} Shaders;

typedef struct Buffers
{
    unsigned VAO;
    unsigned VBO;
    unsigned EBO;
} Buffers;

typedef struct Textures
{
    unsigned max_size;
    unsigned max_depth;
} Textures;

typedef struct Renderer
{
    Camera camera;

    Shaders shaders;
    Buffers buffers;
    Textures textures;

    Window *win; // Borrowed

    bool inited;
} Renderer;

void renderer_create(Renderer *self, Window *win);
void renderer_init(Renderer *self);
void renderer_destroy(Renderer const *self);

bool window_and_renderer_create(Window *win, Renderer *ren, char const *title, unsigned width, unsigned height);

void renderer_set_viewport(Renderer const *self, unsigned width, unsigned height);

void renderer_draw_color(Renderer const *self, unsigned char const rgba[4]);
void renderer_draw_texture(Renderer const *self, unsigned texture, unsigned width, unsigned height, unsigned depth);

void renderer_present(Renderer const *self);

static void render_drain_errors()
{
    GLenum error = GL_NO_ERROR;
    while ((error = glGetError()) != GL_NO_ERROR)
        fprintf(stderr, "OpenGL Error: 0x%04X", error);
    if (error != GL_NO_ERROR)
        abort();
}

unsigned texture_create(Renderer const *ren, unsigned width, unsigned height, unsigned depth, unsigned char channels,
                        unsigned char const *pixels);
void texture_destroy(unsigned texture);

void texture_toggle_filter(Renderer const *ren, unsigned texture);

#ifdef NDEBUG
#define renderer_assert_inited(self) (void)(0)
#else
#define renderer_assert_inited(self)                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        assert((self)->win != nullptr);                                                                                \
        assert((self)->shaders.uniforms.mvp != -1);                                                                    \
        assert((self)->shaders.uniforms.depth != -1);                                                                  \
        assert((self)->camera.zoom != 0);                                                                              \
        assert((int)(self)->camera.angle % 90 == 0);                                                                   \
        assert((self)->shaders.program != 0);                                                                          \
        assert((self)->buffers.VAO != 0);                                                                              \
        assert((self)->buffers.VBO != 0);                                                                              \
        assert((self)->buffers.EBO != 0);                                                                              \
        assert((self)->textures.max_size != 0);                                                                        \
        assert((self)->textures.max_depth != 0);                                                                       \
        assert((self)->inited == true);                                                                                \
    } while (0)
#endif

#ifdef NDEBUG
#define renderer_assert_uninited(self) (void)(0)
#else
#define renderer_assert_uninited(self)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        assert((self)->win != nullptr);                                                                                \
        assert((self)->shaders.uniforms.mvp == 0);                                                                     \
        assert((self)->shaders.uniforms.depth == 0);                                                                   \
        assert((self)->shaders.program == 0);                                                                          \
        assert((self)->buffers.VAO == 0);                                                                              \
        assert((self)->buffers.VBO == 0);                                                                              \
        assert((self)->buffers.EBO == 0);                                                                              \
        assert((self)->textures.max_size == 0);                                                                        \
        assert((self)->textures.max_depth == 0);                                                                       \
        assert((self)->inited == false);                                                                               \
    } while (0)
#endif

#ifdef NDEBUG
#define renderer_assert(self) (void)(0)
#else
#define renderer_assert(self)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((self)->inited)                                                                                            \
            renderer_assert_inited(self);                                                                              \
        else                                                                                                           \
            renderer_assert_uninited(self);                                                                            \
    } while (0)
#endif

#endif // CIVYR_RENDERER_H
