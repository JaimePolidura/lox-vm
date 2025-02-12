#include "os_utils.h"

#ifdef _WIN32
    #include <windows.h>
    #include <dbghelp.h>
    // Link with dbghelp.lib
    #pragma comment(lib, "dbghelp.lib")
#else
    #include <dlfcn.h>
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

uint8_t * allocate_executable(size_t size) {
#ifdef _WIN32
    return (uint8_t *) VirtualAlloc(NULL,
        size,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
#else
    return (uint8_t *) mmap(NULL,
        size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);
#endif
}

void sleep_ms(uint64_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

char * get_function_name_by_ptr(void * function_address) {
#ifdef _WIN32
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    BYTE symbol_buffer [sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    SYMBOL_INFO * symbol = (SYMBOL_INFO *) symbol_buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    SymFromAddr(process, (DWORD64) function_address, NULL, symbol);

    SymCleanup(process);

    return symbol->Name;
#else
    Dl_info info;
    dladdr((void *) function_address, &info);
    return info.dli_sname;
#endif
}
