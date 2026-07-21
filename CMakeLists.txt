cmake_minimum_required(VERSION 4.0)
project(civyr C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(CIVYR_BUILD_ASAN "Build Address Sanitizers" OFF)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if (ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif ()

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options("$<$<CONFIG:RELEASE>:-O3>")
    add_compile_options("$<$<CONFIG:RELEASE>:-ffunction-sections>" "$<$<CONFIG:RELEASE>:-fdata-sections>")

    if (APPLE)
        add_link_options("$<$<CONFIG:RELEASE>:-Wl,-dead_strip>")
    else ()
        add_link_options("$<$<CONFIG:RELEASE>:-Wl,--gc-sections>" "$<$<CONFIG:RELEASE>:-s>")
    endif ()

    if (CIVYR_ENABLE_ASAN AND UNIX)
        add_compile_options(-fsanitize=address,undefined,leak)
        add_link_options(-fsanitize=address,undefined,leak)
    endif ()
endif ()

set(CPACK_PACKAGE_NAME "civyr")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")

if (WIN32)
    set(CPACK_GENERATOR "NSIS")
elseif (APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else ()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
endif ()

include(CPack)

find_package(PkgConfig REQUIRED)
include(FetchContent)

find_package(OpenGL REQUIRED)
pkg_check_modules(VIPS REQUIRED vips)

set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        EXCLUDE_FROM_ALL
)
FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG v2.0.8
        EXCLUDE_FROM_ALL
)
FetchContent_Declare(
        cglm
        GIT_REPOSITORY https://github.com/recp/cglm.git
        GIT_TAG v0.9.6
        EXCLUDE_FROM_ALL
)
FetchContent_Declare(
        log_c_root
        GIT_REPOSITORY https://github.com/rxi/log.c.git
        GIT_TAG master
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glfw glad cglm log_c_root)

include(${glad_SOURCE_DIR}/cmake/GLADconfig.cmake)
glad_add_library(glad_gl_core_33 STATIC REPRODUCIBLE API gl:core=3.3 EXTENSIONS NONE)

add_library(log_c STATIC ${log_c_root_SOURCE_DIR}/src/log.c)
target_include_directories(log_c PUBLIC ${log_c_root_SOURCE_DIR}/src)
target_compile_definitions(log_c PUBLIC LOG_USE_COLOR)

add_subdirectory(src)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
