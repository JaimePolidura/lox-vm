#include "os_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

uint64_t time_millis() {
#ifdef _WIN32
    SYSTEMTIME system_time;
    GetSystemTime(&system_time);
    return ((unsigned long long)system_time.wHour * 3600000 +
            (unsigned long long)system_time.wMinute * 60000 +
            (unsigned long long)system_time.wSecond * 1000 +
            (unsigned long long)system_time.wMilliseconds);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((unsigned long long)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000);
#endif
}
