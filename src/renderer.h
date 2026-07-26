#ifndef CIVYR_RENDERER_H
#define CIVYR_RENDERER_H

#include <stdlib.h>

#include <GLFW/glfw3.h>

#include "log.h"

typedef struct Camera
{
    double zoom;
    double x;
    double y;
    double angle;
    bool mirror_x;
    bool mirror_y;
} Camera;

typedef struct Buffers
{
    unsigned VAO;
    unsigned VBO;
    unsigned EBO;
} Buffers;

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

typedef struct Textures
{
    unsigned max_size;
    unsigned max_depth;
} Textures;

struct Renderer
{
    Camera camera;

    Buffers buffers;
    Shaders shaders;
    Textures textures;

    GLFWwindow *win;

    bool initialized;
};

typedef struct Renderer Renderer;

void renderer_create(Renderer *self, GLFWwindow *win);
void renderer_init(Renderer *self);
void renderer_destroy(Renderer const *self);

void renderer_set_viewport(Renderer *self, int width, int height);

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

#define renderer_assert_initialized(self)                                                                              \
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
    } while (0)

#define renderer_assert_uninitialized(self)                                                                            \
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
    } while (0)

#define renderer_assert(self)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((self)->initialized)                                                                                       \
            renderer_assert_initialized(self);                                                                         \
        else                                                                                                           \
            renderer_assert_uninitialized(self);                                                                       \
    } while (0)

#endif // CIVYR_RENDERER_H
