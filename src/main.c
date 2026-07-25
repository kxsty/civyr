#include <assert.h>
#include <stdlib.h>

#include "app.h"
#include "log.h"

static char *bind_path(int const argc, char *const argv[])
{
    if (argc < 2)
        return nullptr;
    assert(argv[1]);

    char *const path = argv[1];
    if (path[0] == '\0')
        return nullptr;

    return path;
}

int main(int const argc, char *const argv[])
{
    LOG(LOG_TRACE, "Binding image path");
    char const *const path = bind_path(argc, argv);

    App app;
    LOG(LOG_TRACE, "Creating an app");
    app_create(&app, argv[0], path);

    LOG(LOG_TRACE, "Entering main loop");
    while (!glfwWindowShouldClose(app.win.base))
    {
        switch (app.img.state)
        {
        case IMAGE_STATE_UPLOADED:
            if (app.img.recenter)
                app_center_image(&app);
            if (app.img.rerender)
                app_render_image(&app);
            break;
        case IMAGE_STATE_LOADED:
            app_upload_image(&app);
            glfwPostEmptyEvent();
            break;
        case IMAGE_STATE_UNLOADED:
            break;
        }

        if (app.img.state < IMAGE_STATE_UPLOADED || app.img.spec.type == IMAGE_TYPE_STATIC)
        {
            glfwWaitEvents();
            continue;
        }

        long long const now_ms = time_now_ms();

        double timeout = 0;
        if (image_next_frame_ns(&app.img) > now_ms)
            timeout = (double)(image_next_frame_ns(&app.img) - now_ms) / 1000.0;

        glfwWaitEventsTimeout(timeout);
        app.img.rerender = true;
    }

    app_destroy(&app);
    return EXIT_SUCCESS;
}
