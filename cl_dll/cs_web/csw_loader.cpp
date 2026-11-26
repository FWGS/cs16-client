// Loading of the CSWeb library (dlopen / LoadLibrary), symbol binding.

#include "csw_internal.h"
#include "csw_platform.h"
#include "csw_interface.h"
#include "csw_cvars.h"
#include "csw_log.h"

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

namespace csw
{

Cvars cv;
State g;

// The CSWeb library sits next to the client library.
static std::string ModuleDir()
{
    std::string path;
#if defined( _WIN32 )
    HMODULE mod = nullptr;
    char buf[MAX_PATH] = {};
    if( GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&ModuleDir, &mod ) && GetModuleFileNameA( mod, buf, MAX_PATH ) )
        path = buf;
    size_t slash = path.find_last_of( "\\/" );
#else
    Dl_info info;
    if( dladdr( (void *)&ModuleDir, &info ) && info.dli_fname )
        path = info.dli_fname;
    size_t slash = path.rfind( '/' );
#endif
    return slash != std::string::npos ? path.substr( 0, slash ) : std::string( "." );
}

static bool OpenLibrary( const std::string &dir )
{
    const std::string path = dir + "/" + CSWEB_LIB_FILE;

    Debug("ModuleDir: %s", dir.c_str());
    Debug("loading: %s", path.c_str());

#if defined( _WIN32 )
    SetDllDirectoryA( dir.c_str() );
    g.lib = (void *)LoadLibraryExA( path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );

    if( !g.lib )
        Error( "LoadLibrary(%s) failed, error %lu", path.c_str(), GetLastError() );
#else
#if defined( __ANDROID__ )
    static void *cxxRuntime = nullptr;
    if( !cxxRuntime )
    {
        const std::string cxxPath = dir + "/libc++_shared.so";
        cxxRuntime = dlopen( cxxPath.c_str(), RTLD_NOW | RTLD_GLOBAL );

        if( !cxxRuntime )
        {
            Error( "dlopen(%s) failed: %s", cxxPath.c_str(), dlerror() );
            return false;
        }

        Info( "^2libc++_shared.so loaded^7" );
    }
#endif
    g.lib = dlopen( path.c_str(), RTLD_NOW | RTLD_LOCAL );

    if( !g.lib )
        Error( "dlopen(%s) failed: %s", path.c_str(), dlerror() );
#endif

    return g.lib != nullptr;
}

static void CloseLibrary()
{
    g.api = {};
    g.apiReady = false;

    if( !g.lib )
        return;

#if defined( _WIN32 )
    FreeLibrary( (HMODULE)g.lib );
#else
    dlclose( g.lib );
#endif
    g.lib = nullptr;
}

static bool BindSymbols()
{
#define BIND( field, sym ) \
    do \
    { \
        g.api.field = reinterpret_cast<decltype( g.api.field )>( CSW_LIB_SYM( g.lib, sym ) ); \
        if( !g.api.field ) \
        { \
            Error( "missing symbol %s", sym ); \
            return false; \
        } \
    } while( 0 )

    BIND( abi_version, "cs_web_abi_version" );
    BIND( set_debug_level, "cs_web_set_debug_level" );
    BIND( create, "cs_web_create" );
    BIND( destroy, "cs_web_destroy" );
    BIND( resize, "cs_web_resize" );
    BIND( load_url, "cs_web_load_url" );
    BIND( acquire, "cs_web_acquire" );
    BIND( mouse_move, "cs_web_mouse_move" );
    BIND( mouse_button, "cs_web_mouse_button" );
    BIND( wheel, "cs_web_wheel" );
    BIND( key, "cs_web_key" );

#undef BIND
    return true;
}

#if defined( __ANDROID__ ) && ( defined( __aarch64__ ) || defined( __arm__ ) )
// SpiderMonkey cannot hold tagged heap pointers (top byte 0xB4), so tagging must be off.
// Also needs android:allowNativeHeapPointerTagging="false" in the engine's AndroidManifest.xml:
// https://github.com/FWGS/xash3d-fwgs/blob/master/android/app/src/main/AndroidManifest.xml
// More details: https://source.android.com/docs/security/test/tagged-pointers
static void DisableHeapTagging()
{
    typedef int ( *mallopt_fn )( int, int );

    if( mallopt_fn fn = (mallopt_fn)dlsym( RTLD_DEFAULT, "mallopt" ) )
        Debug( "heap tagging off: %d", fn( M_BIONIC_SET_HEAP_TAGGING_LEVEL, M_HEAP_TAGGING_LEVEL_NONE ) );
}
#endif

static bool LoadApi()
{
    if( g.apiReady )
        return true;

    if( g.lib ) // a previous attempt failed half way
        return false;

    RegisterCvars(); // before anything is logged, so csweb_debug is respected

    if( !OpenLibrary( ModuleDir() ) )
        return false;

    if( !BindSymbols() )
    {
        CloseLibrary();
        return false;
    }

    if( g.api.abi_version() != CS_WEB_ABI_VERSION )
    {
        Error( "ABI mismatch: library %u, client %u", g.api.abi_version(), (unsigned)CS_WEB_ABI_VERSION );
        CloseLibrary();
        return false;
    }

    g.apiReady = true;

#if defined( __ANDROID__ ) && defined( __aarch64__ )
    DisableHeapTagging();
#endif

    Info( "^2library loaded^7 ABI ver. %u", (unsigned)CS_WEB_ABI_VERSION );
    return true;
}

} // namespace csw

void CSW_Init()
{
    if( csw::LoadApi() )
        csw::SyncDebug();
}