add_library(civyr_lib
        app.c
        image.c
        renderer.c
        window.c
)

target_include_directories(civyr_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${VIPS_INCLUDE_DIRS}
)

target_link_libraries(civyr_lib
        PUBLIC
        OpenGL::GL
        glad_gl_core_33
        glfw
        cglm_headers
        log_c
        ${VIPS_LIBRARIES}
)

target_compile_options(civyr_lib
        PRIVATE
        ${VIPS_CFLAGS_OTHER}
)

if (WIN32)
    target_link_libraries(civyr_lib PRIVATE dwmapi)
endif ()

if (WIN32)
    add_executable(civyr WIN32
            main.c
    )
else ()
    add_executable(civyr
            main.c
    )
endif ()

target_link_libraries(civyr PRIVATE civyr_lib)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

install(TARGETS civyr
        RUNTIME_DEPENDENCIES
        DIRECTORIES
            ${VIPS_LIBRARY_DIRS}
            ${VIPS_LIBDIR}
            "${VIPS_LIBDIR}/../bin"

        PRE_EXCLUDE_REGEXES
            "^api-ms-.*"
            "^ext-ms-.*"
            "^AzureAttest.*"
            "[/\\\\]HvsiFileTrust\\.dll$"
            "[/\\\\]PdmUtilities\\.dll$"
            "[/\\\\]wpaxholder\\.dll$"
            "[/\\\\]d3d9\\.dll$"
            "[/\\\\]dxgi\\.dll$"
            "[/\\\\]bcryptprimitives\\.dll$"
            "[/\\\\]ntdll\\.dll$"

        POST_EXCLUDE_REGEXES
            "^.:/[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/.*"

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
)