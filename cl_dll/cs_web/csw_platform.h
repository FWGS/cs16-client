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