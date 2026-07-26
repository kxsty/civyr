#ifndef CIVYR_IMAGE_H
#define CIVYR_IMAGE_H

#include "renderer.h"
#include "utils.h"

typedef enum ImageState
{
    IMAGE_STATE_UNLOADED,
    IMAGE_STATE_LOADED,
    IMAGE_STATE_UPLOADED
} ImageState;

typedef enum ImageType
{
    IMAGE_TYPE_STATIC,
    IMAGE_TYPE_ANIMATED
} ImageType;

typedef struct StaticImage
{
} StaticImage;

typedef struct AnimatedImage
{
    int *delays_ms;
    long long next_frame_ms;
    int count;
    int curr_frame;
} AnimatedImage;

typedef struct SpecificImage
{
    ImageType type;

    union {
        StaticImage stat;
        AnimatedImage anim;
    };
} SpecificImage;

typedef struct Image
{
    _Atomic ImageState state;

    char *path; // UNLOADED

    unsigned width;         // LOADED
    unsigned height;        // LOADED
    unsigned char channels; // LOADED
    unsigned char *pixels;  // LOADED
    SpecificImage spec;     // LOADED

    unsigned texture; // UPLOADED
    bool recenter;    // UPLOADED
    bool rerender;    // UPLOADED
} Image;

void image_create(Image *self, char const *path);
void image_destroy(Image *self);

static int image_frame_count(Image const *self)
{
    if (self->state < IMAGE_STATE_LOADED)
        panic("State mismatch");

    switch (self->spec.type)
    {
    case IMAGE_TYPE_STATIC:
        return 1;
    case IMAGE_TYPE_ANIMATED:
        return self->spec.anim.count;
    default:
        panic("Unreachable");
    }
}

static int image_curr_frame(Image const *self)
{
    if (self->state < IMAGE_STATE_LOADED)
        panic("State mismatch");

    switch (self->spec.type)
    {
    case IMAGE_TYPE_STATIC:
        return 1;
    case IMAGE_TYPE_ANIMATED:
        return self->spec.anim.curr_frame;
    default:
        panic("Unreachable");
    }
}

static long long image_next_frame_ns(Image const *self)
{
    if (self->state < IMAGE_STATE_LOADED)
        panic("State mismatch");

    if (self->spec.type != IMAGE_TYPE_ANIMATED)
        panic("Type mismatch");

    return self->spec.anim.next_frame_ms;
}

bool image_load(Image *self);
void image_load_detached(Image *self);
void image_upload(Image *self, Renderer const *ren);

void image_file_get_size(char const *path, int *width, int *height);

#ifdef NDEBUG
#define static_image_assert(self) (void)(0)
#else
#define static_image_assert(self) assert((self) != nullptr);
#endif

#ifdef NDEBUG
#define animated_image_assert(self) (void)(0)
#else
#define animated_image_assert(self)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        assert((self)->delays_ms != nullptr);                                                                          \
    } while (0)
#endif

#ifdef NDEBUG
#define specific_image_assert(self) (void)(0)
#else
#define specific_image_assert(self)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        switch ((self)->type)                                                                                          \
        {                                                                                                              \
        case IMAGE_TYPE_STATIC:                                                                                        \
            static_image_assert(&(self)->stat);                                                                        \
            break;                                                                                                     \
        case IMAGE_TYPE_ANIMATED:                                                                                      \
            animated_image_assert(&(self)->anim);                                                                      \
            break;                                                                                                     \
        }                                                                                                              \
    } while (0)
#endif

#ifdef NDEBUG
#define image_assert_unloaded(self) (void)(0)
#else
#define image_assert_unloaded(self)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        assert((self)->state == IMAGE_STATE_UNLOADED);                                                                 \
        assert((self)->path == nullptr || *((self)->path) != '\0');                                                    \
        assert((self)->width == 0);                                                                                    \
        assert((self)->height == 0);                                                                                   \
        assert((self)->channels == 0);                                                                                 \
        assert((self)->pixels == nullptr);                                                                             \
        assert((self)->spec.type == IMAGE_TYPE_STATIC);                                                                \
        static_image_assert(&(self)->spec.stat);                                                                       \
        assert((self)->texture == 0);                                                                                  \
        assert((self)->recenter == false);                                                                             \
        assert((self)->rerender == false);                                                                             \
    } while (0)
#endif

#ifdef NDEBUG
#define image_assert_loaded(self) (void)(0)
#else
#define image_assert_loaded(self)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        assert((self)->state == IMAGE_STATE_LOADED);                                                                   \
        assert((self)->path == nullptr || *((self)->path) != '\0');                                                    \
        assert((self)->width > 0);                                                                                     \
        assert((self)->height > 0);                                                                                    \
        assert((self)->channels > 0);                                                                                  \
        assert((self)->pixels != nullptr);                                                                             \
        specific_image_assert(&(self)->spec);                                                                          \
        assert((self)->texture == 0);                                                                                  \
        assert((self)->recenter == false);                                                                             \
        assert((self)->rerender == false);                                                                             \
    } while (0)
#endif

#ifdef NDEBUG
#define image_assert_uploaded(self) (void)(0)
#else
#define image_assert_uploaded(self)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((self) != nullptr);                                                                                     \
        assert((self)->state == IMAGE_STATE_UPLOADED);                                                                 \
        assert((self)->path == nullptr || *((self)->path) != '\0');                                                    \
        assert((self)->width > 0);                                                                                     \
        assert((self)->height > 0);                                                                                    \
        assert((self)->channels > 0);                                                                                  \
        assert((self)->pixels == nullptr);                                                                             \
        specific_image_assert(&(self)->spec);                                                                          \
        assert((self)->texture != 0);                                                                                  \
    } while (0)
#endif

#ifdef NDEBUG
#define image_assert(self) (void)(0)
#else
#define image_assert(self)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        switch ((self)->state)                                                                                         \
        {                                                                                                              \
        case IMAGE_STATE_UNLOADED:                                                                                     \
            image_assert_unloaded(self);                                                                               \
            break;                                                                                                     \
        case IMAGE_STATE_LOADED:                                                                                       \
            image_assert_loaded(self);                                                                                 \
            break;                                                                                                     \
        case IMAGE_STATE_UPLOADED:                                                                                     \
            image_assert_uploaded(self);                                                                               \
            break;                                                                                                     \
        }                                                                                                              \
    } while (0)
#endif

#endif // CIVYR_IMAGE_H
