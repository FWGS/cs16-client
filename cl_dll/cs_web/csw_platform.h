#pragma once

#include <string>

#if defined( _WIN32 )
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #define HSPRITE WINDOWS_HSPRITE
    #include <windows.h>
    #undef HSPRITE
    #define CSW_LIB_SYM( h, n ) ( (void *)GetProcAddress( (HMODULE)( h ), n ) )
#else
    #include <dlfcn.h>
    #if defined( __ANDROID__ )
        #include <malloc.h> // mallopt, M_BIONIC_SET_HEAP_TAGGING_LEVEL
    #endif
    #define CSW_LIB_SYM( h, n ) dlsym( h, n )
#endif

#if defined(__ANDROID__) && defined(__aarch64__)
    #define CSWEB_LIB_FILE "libcsweb_android_arm64.so"

#elif defined(__ANDROID__) && defined(__arm__)
    #define CSWEB_LIB_FILE "libcsweb_android_armv7.so"

#elif defined(__ANDROID__) && defined(__x86_64__)
    #define CSWEB_LIB_FILE "libcsweb_android_amd64.so"

#elif defined(__ANDROID__) && defined(__i386__)
    #define CSWEB_LIB_FILE "libcsweb_android_i686.so"

#elif defined(_WIN64)
    #define CSWEB_LIB_FILE "csweb_amd64.dll"

#elif defined(_WIN32)
    #define CSWEB_LIB_FILE "csweb.dll"

#elif defined(__APPLE__) && defined(__aarch64__)
    #define CSWEB_LIB_FILE "csweb_arm64.dylib"

#elif defined(__APPLE__) && defined(__x86_64__)
    #define CSWEB_LIB_FILE "csweb_amd64.dylib"

#elif defined(__linux__) && defined(__aarch64__)
    #define CSWEB_LIB_FILE "csweb_arm64.so"

#elif defined(__linux__) && defined(__x86_64__)
    #define CSWEB_LIB_FILE "csweb_amd64.so"

#elif defined(__linux__) && defined(__arm__)
    #define CSWEB_LIB_FILE "csweb_arm.so"

#elif defined(__linux__) && defined(__i386__)
    #define CSWEB_LIB_FILE "csweb.so"

#else
    #error "Unsupported platform or architecture"
#endif