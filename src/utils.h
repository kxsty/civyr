#ifndef CIVYR_SHARED_H
#define CIVYR_SHARED_H

#include <time.h>

#define MSEC_PER_SEC 1000LL
#define NSEC_PER_MSEC 1000000LL
#define NSEC_PER_SEC 1000000000LL

static long long time_now_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * MSEC_PER_SEC + ts.tv_nsec / NSEC_PER_MSEC;
}
static long long time_now_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}
static long long time_since_ms(long long const ms)
{
    return time_now_ms() - ms;
}

#ifdef NDEBUG
#define LOG(level, fmt, ...) ((void)0)
#else
#define LOG(level, fmt, ...) log_log(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif

static char const *path_basename(char const *const path)
{
#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

    if (path == nullptr)
        return nullptr;

    char const *basename = path;

    char const *curr = path;
    while (*curr != '\0')
    {
        if (*curr == PATH_SEPARATOR)
            basename = curr + 1;
        curr++;
    }

    return basename;
}

#define panic(fmt, ...)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        fprintf(stderr, "Panic: " fmt ", file %s, line %d\n", ##__VA_ARGS__, __FILE__, __LINE__);                      \
        abort();                                                                                                       \
    } while (0)

#endif // CIVYR_SHARED_H