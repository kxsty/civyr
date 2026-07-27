#include <assert.h>

#include <cglm/cglm.h>
#include <glad/gl.h>

#include "app.h"
#include "renderer.h"
#include "utils.h"
#include "window.h"

static char constexpr vertexShaderSource[] = "#version 330 core\n"
                                             "layout (location = 0) in vec2 a_pos;\n"
                                             "layout (location = 1) in vec2 a_texCoords;\n"
                                             "out vec2 v_texCoords;\n"
                                             "uniform mat4 u_mvp;\n"
                                             "void main() {\n"
                                             "    gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);\n"
                                             "    v_texCoords = a_texCoords;\n"
                                             "}";

static char constexpr fragmentShaderSource[] = "#version 330 core\n"
                                               "out vec4 FragColor;\n"
                                               "in vec2 v_texCoords;\n"
                                               "uniform sampler2DArray u_texture;\n"
                                               "uniform float u_depth;\n"
                                               "void main() {\n"
                                               "    FragColor = texture(u_texture, vec3(v_texCoords, u_depth));\n"
                                               "}";

static unsigned shader_compile(unsigned const type, char const *const source)
{
    assert(source != nullptr && *source != '\0');

    unsigned const shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        panic("Shader compilation failed: %s\n", infoLog);
    }

    return shader;
}

static void uniforms_init(Uniforms *const self, unsigned const shader_program)
{
    assert(self != nullptr && shader_program != 0);

    self->mvp = glGetUniformLocation(shader_program, "u_mvp");
    self->depth = glGetUniformLocation(shader_program, "u_depth");
}

static void shaders_init(Shaders *const shaders)
{
    assert(shaders != nullptr);

    unsigned const vertex_shader = shader_compile(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned const fragment_shader = shader_compile(GL_FRAGMENT_SHADER, fragmentShaderSource);
    shaders->program = glCreateProgram();

    glAttachShader(shaders->program, vertex_shader);
    glAttachShader(shaders->program, fragment_shader);
    glLinkProgram(shaders->program);

    int success;
    glGetProgramiv(shaders->program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(shaders->program, sizeof(infoLog), nullptr, infoLog);
        panic("Shader program linking failed: %s\n", infoLog);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    uniforms_init(&shaders->uniforms, shaders->program);
}

static void shaders_destroy(Shaders const *const shaders)
{
    assert(shaders != nullptr);

    glDeleteProgram(shaders->program);
}

static void buffers_init(Buffers *const self)
{
    assert(self != nullptr);

    float constexpr vertices[] = {
        0.5f,  0.5f,  1.0f, 1.0f, //
        0.5f,  -0.5f, 1.0f, 0.0f, //
        -0.5f, -0.5f, 0.0f, 0.0f, //
        -0.5f, 0.5f,  0.0f, 1.0f, //
    };
    unsigned const indices[] = {
        0, 1, 3, //
        1, 2, 3, //
    };

    glGenVertexArrays(1, &self->VAO);
    glBindVertexArray(self->VAO);

    glGenBuffers(1, &self->VBO);
    glBindBuffer(GL_ARRAY_BUFFER, self->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &self->EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)(2 * sizeof(float))); // NOLINT(*-no-int-to-ptr)
    glEnableVertexAttribArray(1);
}

static void buffers_destroy(Buffers const *const self)
{
    assert(self != nullptr);

    if (self->VAO)
        glDeleteVertexArrays(1, &self->VAO);
    if (self->VBO)
        glDeleteBuffers(1, &self->VBO);
    if (self->EBO)
        glDeleteBuffers(1, &self->EBO);
}

static void textures_init(Textures *const self)
{
    assert(self != nullptr);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, (int *)&self->max_size);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, (int *)&self->max_depth);
}

static void matrix_model(mat4 dest, float const tex_w, float const tex_h, float const angle, bool const mirror_x,
                         bool const mirror_y)
{
    assert(dest != nullptr && tex_w > 0 && tex_h > 0);

    glm_mat4_identity(dest);

    glm_rotate(dest, glm_rad(angle), (vec3){0.0f, 0.0f, 1.0f});

    float const sx = tex_w * (mirror_x ? -1.0f : 1.0f);
    float const sy = tex_h * (mirror_y ? -1.0f : 1.0f);

    glm_scale(dest, (vec3){sx, sy, 1.0f});
}

static void matrix_view(mat4 dest, float const x, float const y)
{
    assert(dest != nullptr);

    glm_mat4_identity(dest);

    glm_translate(dest, (vec3){-x, y, 0.0f});
}

static void matrix_projection(mat4 dest, float const win_w, float const win_h, float const zoom)
{
    assert(dest != nullptr && win_w > 0 && win_h > 0 && zoom > 0);

    float const half_w = win_w * 0.5f / zoom;
    float const half_h = win_h * 0.5f / zoom;

    glm_ortho(-half_w, half_w, half_h, -half_h, -1.0f, 1.0f, dest);
}

static void matrix_mvp(mat4 dest, float const tex_w, float const tex_h, float const win_w, float const win_h,
                       Camera const cam)
{
    mat4 model, view, proj, vp;

    matrix_model(model, tex_w, tex_h, (float)cam.angle, cam.mirror_x, cam.mirror_y);
    matrix_view(view, (float)cam.x, (float)cam.y);
    matrix_projection(proj, win_w, win_h, (float)cam.zoom);

    glm_mat4_mul(proj, view, vp);
    glm_mat4_mul(vp, model, dest);
}

static int texture_channels_to_gl_format(unsigned char const channels)
{
    assert(channels > 0);

    switch (channels)
    {
    case 1:
        return GL_RED;
    case 2:
        return GL_RG;
    case 4:
        return GL_RGBA;
    case 3:
    default:
        return GL_RGB;
    }
}

void renderer_create(Renderer *const self, Window *const win)
{
    assert(self != nullptr && win != nullptr);

    glfwMakeContextCurrent(win->base);
    if (!gladLoadGL(glfwGetProcAddress))
        panic("Failed to initialize GLAD");

    *self = (Renderer){
        .win = win,
        .camera =
            {
                .zoom = -1,
                .x = 0,
                .y = 0,
                .angle = 0,
            },
        .inited = false,
    };
}

void renderer_init(Renderer *const self)
{
    renderer_assert_uninited(self);

    shaders_init(&self->shaders);

    buffers_init(&self->buffers);

    textures_init(&self->textures);

    self->inited = true;
}

void renderer_destroy(Renderer const *const self)
{
    assert(self != nullptr);

    if (!self->inited)
        return;

    buffers_destroy(&self->buffers);
    shaders_destroy(&self->shaders);
}

bool window_and_renderer_create(Window *const win, Renderer *const ren, char const *const title, unsigned const width,
                                unsigned const height)
{
    assert(ren != nullptr && win != nullptr && title != nullptr && *title != '\0' && width > 0 && height > 0);

    GLFWwindow *base_win;
    if (!base_window_create(&base_win, title, width, height, false))
        return false;

    window_create(win, base_win);

    LOG(LOG_TRACE, "Creating renderer");
    renderer_create(ren, win);

    LOG(LOG_TRACE, "Rendering background");
    renderer_draw_color(ren, BG_RGBA);
    renderer_present(ren);

    LOG(LOG_TRACE, "Showing window");
    glfwShowWindow(base_win);

    renderer_init(ren);

    base_window_init(base_win);

    return true;
}

void renderer_set_viewport(Renderer const *const self, unsigned const width, unsigned const height)
{
    renderer_assert_inited(self);
    assert(width > 0 && height > 0);

    glViewport(0, 0, (int)width, (int)height);
}

void renderer_draw_color(Renderer const *const self, unsigned char const rgba[4])
{
    renderer_assert(self);

    glClearColor((float)rgba[0] / 255.0f, (float)rgba[1] / 255.0f, (float)rgba[2] / 255.0f, (float)rgba[3] / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_draw_texture(Renderer const *const self, unsigned const texture, unsigned const width,
                           unsigned const height, unsigned const depth)
{
    renderer_assert_inited(self);
    assert(texture > 0 && width > 0 && height > 0);

    glUseProgram(self->shaders.program);

    mat4 mvp;
    matrix_mvp(mvp, (float)width, (float)height, (float)self->win->width, (float)self->win->height, self->camera);

    glUniformMatrix4fv(self->shaders.uniforms.mvp, 1, GL_FALSE, (float *)mvp);
    glUniform1f(self->shaders.uniforms.depth, (float)depth);

    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glBindVertexArray(self->buffers.VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void renderer_present(Renderer const *const self)
{
    renderer_assert(self);

    glfwSwapBuffers(self->win->base);
}

unsigned texture_create(Renderer const *const ren, unsigned const width, unsigned const height, unsigned const depth,
                        unsigned char const channels, unsigned char const *const pixels)
{
    renderer_assert_inited(ren);
    assert(width > 0 && height > 0);

    if (width > ren->textures.max_size || height > ren->textures.max_size)
        panic("Texture size too large: %d x %d", width, height);

    if (depth > ren->textures.max_depth)
        panic("Texture depth too large: %d", depth);

    unsigned texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_LOD_BIAS, -0.5f);

    int const format = texture_channels_to_gl_format(channels);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, format, (int)width, (int)height, (int)depth, 0, format, GL_UNSIGNED_BYTE,
                 pixels);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    render_drain_errors();
    return texture;
}

void texture_destroy(unsigned const texture)
{
    assert(texture != 0);

    if (texture)
        glDeleteTextures(1, &texture);
}

void texture_toggle_filter(Renderer const *const ren, unsigned const texture)
{
    renderer_assert_inited(ren);
    assert(texture != 0);

    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    GLint minFilter, magFilter;
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, &minFilter);
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, &magFilter);

    if (minFilter == GL_LINEAR_MIPMAP_LINEAR && magFilter == GL_LINEAR)
    {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else if (minFilter == GL_NEAREST_MIPMAP_NEAREST && magFilter == GL_NEAREST)
    {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        panic("Invalid texture filter");
    }
}
