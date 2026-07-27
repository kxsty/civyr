#ifndef CIVYR_UTLIS_H
#define CIVYR_UTLIS_H

#include <time.h>

#define MSEC_PER_SEC 1000
#define NSEC_PER_MSEC 1000000LL
#define NSEC_PER_SEC 1000000000LL

static long long time_now_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}
static long long time_since_ns(long long const since_ns)
{
    return time_now_ns() - since_ns;
}

#ifdef NDEBUG
#define LOG(level, fmt, ...)                                                                                           \
    ((level) >= LOG_ERROR ? log_log((level), __FILE__, __LINE__, (fmt), ##__VA_ARGS__) : ((void)0))
#else
#define LOG(level, fmt, ...) log_log((level), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
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
        fflush(stderr);                                                                                                \
        abort();                                                                                                       \
    } while (0)

#define die(fmt, ...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        fprintf(stderr, "Panic: " fmt ", file %s, line %d\n", ##__VA_ARGS__, __FILE__, __LINE__);                      \
        fflush(stderr);                                                                                                \
        exit(EXIT_FAILURE);                                                                                            \
    } while (0)

#endif // CIVYR_UTLIS_H