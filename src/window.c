#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include "app.h"
#include "utils.h"

#ifdef _WIN32

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <windows.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static bool win32_is_dark_mode()
{
    HKEY hKey;
    DWORD value = 1;
    DWORD size = sizeof(value);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&value, &size);
        RegCloseKey(hKey);
    }

    return value == 0;
}

static bool win32_adapt_dark_mode(GLFWwindow *win)
{
    if (!win32_is_dark_mode())
        return true;

    HWND const hwnd = glfwGetWin32Window(win); // NOLINT(*-misplaced-const)
    if (DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &(BOOL){true}, sizeof(BOOL)) != 0)
        return false;

    return true;
}

#endif

#define MIN_WIDTH 160
#define MIN_HEIGHT 120

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

static GLFWmonitor *window_best_monitor(Window const *const self)
{
    window_assert(self);

    int wx, wr, ww, wh;
    glfwGetWindowPos(self->base, &wx, &wr);
    glfwGetWindowSize(self->base, &ww, &wh);

    int count;
    GLFWmonitor *const *const mons = glfwGetMonitors(&count);

    GLFWmonitor *best_mon = nullptr;
    int best_overlap = 0;

    for (int i = 0; i < count; i++)
    {
        int mx, my;
        glfwGetMonitorPos(mons[i], &mx, &my);

        GLFWvidmode const *const mode = glfwGetVideoMode(mons[i]);

        int const overlap = max(0, min(wx + ww, mx + mode->width) - max(wx, mx)) *
                            max(0, min(wr + wh, my + mode->height) - max(wr, my));

        if (overlap > best_overlap)
        {
            best_overlap = overlap;
            best_mon = mons[i];
        }
    }

    return best_mon;
}

static void framebuffer_size_callback(GLFWwindow *const window, int const width, int const height)
{
    App *const app = glfwGetWindowUserPointer(window);
    app_assert(app);

    if (width <= 0 || height <= 0)
        return;

    if (width == app->win.width && height == app->win.height)
        return;

    renderer_set_viewport(&app->ren, width, height);
    app->win.width = width;
    app->win.height = height;
    LOG(LOG_TRACE, "window:{w:%i,h:%i}", app->win.width, app->win.height);

    if (app->img.state < IMAGE_STATE_UPLOADED)
    {
        app_render_bg(app);
        return;
    }

    app_center_image(app);
    app_render_image(app);
}

static void key_callback(GLFWwindow *const window, int const key, [[maybe_unused]] int const scancode, int const action,
                         [[maybe_unused]] int const mods)
{
    if (action == GLFW_RELEASE)
        return;

    App *const app = glfwGetWindowUserPointer(window);
    app_assert(app);

    if (app->img.state < IMAGE_STATE_UPLOADED)
    {
        if (key == GLFW_KEY_F11)
            window_toggle_fullscreen(&app->win);
        return;
    }

    switch (key)
    {
    case GLFW_KEY_LEFT:
        app_rotate_image(app, false);
        break;
    case GLFW_KEY_RIGHT:
        app_rotate_image(app, true);
        break;
    case GLFW_KEY_UP:
        app_mirror_image(app, false, true);
        break;
    case GLFW_KEY_DOWN:
        app_mirror_image(app, true, false);
        break;
    case GLFW_KEY_F11:
        window_toggle_fullscreen(&app->win);
        app->img.rerender = true;
        break;
    case GLFW_KEY_1:
        app->ren.camera.zoom = 1;
        app->img.rerender = true;
        break;
    case GLFW_KEY_F:
        texture_toggle_filter(&app->ren, app->img.texture);
        app->img.rerender = true;
        break;
    case GLFW_KEY_W:
        app_pan_image(app, 0, 10);
        break;
    case GLFW_KEY_A:
        app_pan_image(app, 10, 0);
        break;
    case GLFW_KEY_S:
        app_pan_image(app, 0, -10);
        break;
    case GLFW_KEY_D:
        app_pan_image(app, -10, 0);
        break;
    default:
        break;
    }
}

static void mouse_button_callback(GLFWwindow *const window, int const button, int const action,
                                  [[maybe_unused]] int const mods)
{
    App *const app = glfwGetWindowUserPointer(window);
    app_assert(app);
    if (app->img.state < IMAGE_STATE_UPLOADED)
        return;

    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        app->win.mouse.is_dragging = action != GLFW_RELEASE;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        if (action != GLFW_PRESS)
            break;
        app->img.recenter = true;
        app->img.rerender = true;
        break;
    default:
        break;
    }
}

static void cursor_position_callback(GLFWwindow *const window, double const xpos, double const ypos)
{
    App *const app = glfwGetWindowUserPointer(window);
    app_assert(app);
    if (app->img.state < IMAGE_STATE_UPLOADED)
        return;

    if (!app->win.mouse.is_dragging)
    {
        app->win.mouse.x = xpos;
        app->win.mouse.y = ypos;
        LOG(LOG_TRACE, "mouse:{x:%f,y:%f}", app->win.mouse.x, app->win.mouse.y);
        return;
    }

    double const x_offset = xpos - app->win.mouse.x;
    double const y_offset = ypos - app->win.mouse.y;

    app_pan_image(app, x_offset, y_offset);

    app->win.mouse.x = xpos;
    app->win.mouse.y = ypos;
    LOG(LOG_TRACE, "camera:{x:%f,y:%f}", app->ren.camera.x, app->ren.camera.y);
}

static void scroll_callback(GLFWwindow *const window, [[maybe_unused]] double const xoffset, double const yoffset)
{
    App *const app = glfwGetWindowUserPointer(window);
    app_assert(app);
    if (app->img.state < IMAGE_STATE_UPLOADED)
        return;

    if (app->win.mouse.x == DBL_MIN && app->win.mouse.y == DBL_MIN)
        glfwGetCursorPos(window, &app->win.mouse.x, &app->win.mouse.y);

    app_zoom_image(app, yoffset > 0);
}

static void drop_callback(GLFWwindow *const window, int const path_count, char const **const paths)
{
    if (path_count < 1)
        return;

    char const *const path = paths[0];
    if (!path)
        return;

    App *const app = glfwGetWindowUserPointer(window);
    app_assert(app);

    if (path == app->img.path)
        return;

    if (app->img.path)
    {
        if (strcmp(path, app->img.path) == 0)
            return;

        if (app->img.state < IMAGE_STATE_LOADED)
            return;
    }

    Image img;
    image_create(&img, path);
    image_load(&img);

    Image tmp = app->img;
    app->img = img;
    image_destroy(&tmp);

    glfwPostEmptyEvent();
}

void base_window_create(GLFWwindow **self_p, char const *const title, unsigned const width, unsigned const height,
                        bool const visible)
{
    assert(title != nullptr && *title != '\0' && width > 0 && height > 0);

    LOG(LOG_TRACE, "Setting window hints");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (!visible)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    LOG(LOG_TRACE, "Creating glfw window");
    *self_p = glfwCreateWindow((int)width, (int)height, title, nullptr, nullptr);
    if (!*self_p)
    {
        char const *err;
        glfwGetError(&err);
        panic("Failed to create glfw window: %s", err);
    }

#ifdef _WIN32
    LOG(LOG_TRACE, "Adapting dark mode for the window");
    win32_adapt_dark_mode(*self_p);
#endif
}

void base_window_initialize(GLFWwindow *const self)
{
    assert(self != nullptr);

    LOG(LOG_TRACE, "Setting window limits");
    glfwSetWindowSizeLimits(self, 160, 120, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSwapInterval(1);

    LOG(LOG_TRACE, "Setting window callbacks");
    glfwSetFramebufferSizeCallback(self, framebuffer_size_callback);
    glfwSetKeyCallback(self, key_callback);
    glfwSetMouseButtonCallback(self, mouse_button_callback);
    glfwSetCursorPosCallback(self, cursor_position_callback);
    glfwSetScrollCallback(self, scroll_callback);
    glfwSetDropCallback(self, drop_callback);
}

void window_create(Window *self, GLFWwindow *const base)
{
    assert(self != nullptr && base != nullptr);

    unsigned width, height;
    glfwGetWindowSize(base, (int *)&width, (int *)&height);

    *self = (Window){
        .base = base,
        .mouse =
            {
                .x = DBL_MIN,
                .y = DBL_MIN,
            },
        .width = width,
        .height = height,
    };
}

void window_destroy(Window const *self)
{
    window_assert(self);

    glfwDestroyWindow(self->base);
}

void window_transform(Window const *self, int const x, int const y, unsigned const width, unsigned const height)
{
    window_assert(self);

    glfwSetWindowPos(self->base, x, y);
    if (self->width != width || self->height != height)
        glfwSetWindowSize(self->base, (int)width, (int)height);
}

void window_workarea(Window const *self, int *const x, int *const y, unsigned *const width, unsigned *const height)
{
    window_assert(self);
    assert(x != nullptr && y != nullptr && width != nullptr && height != nullptr);

    GLFWmonitor *best_mon = window_best_monitor(self);

    glfwGetMonitorWorkarea(best_mon, x, y, (int *)width, (int *)height);
}

void window_toggle_fullscreen(Window const *const self)
{
    window_assert(self);

    static int px, py, pw, ph;
    GLFWmonitor *const win_mon = glfwGetWindowMonitor(self->base);
    if (!win_mon)
    {
        GLFWmonitor *const best_mon = window_best_monitor(self);
        GLFWvidmode const *const mode = glfwGetVideoMode(best_mon);
        glfwGetWindowPos(self->base, &px, &py);
        glfwGetWindowSize(self->base, &pw, &ph);
        glfwSetWindowMonitor(self->base, best_mon, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else
    {
        glfwSetWindowMonitor(self->base, nullptr, px, py, pw, ph, GLFW_DONT_CARE);
    }
}

void window_rename(Window const *self, char const *const title)
{
    window_assert(self);
    assert(title != nullptr && *title != '\0');

    glfwSetWindowTitle(self->base, title);
}