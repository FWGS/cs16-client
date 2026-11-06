#if defined(CS16CLIENT_ENABLE_CEF)

#include "chrome_environment.h"
#include "chrome_platform.h"
#include "chrome_debug.h"
#include "chrome_path.h"

#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include "wrapper/cef_library_loader.h"
#endif

namespace iHTMLChrome {
namespace CEF {

bool Platform_GetExecutablePath( std::string& executable_path )
{
#if defined(_WIN32)
	wchar_t wide[MAX_PATH * 2];
	const DWORD n = GetModuleFileNameW( nullptr, wide, sizeof( wide ) / sizeof( wide[0] ) );
	if( n == 0 )
		return false;

	const int bytes = WideCharToMultiByte( CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr );
	if( bytes <= 1 )
		return false;

	executable_path.resize( static_cast<std::size_t>( bytes - 1 ) );
	WideCharToMultiByte( CP_UTF8, 0, wide, -1, executable_path.data(), bytes, nullptr, nullptr );
	return true;
#elif defined(__linux__)
	char buf[PATH_MAX];
	const ssize_t len = readlink( "/proc/self/exe", buf, sizeof( buf ) - 1 );
	if( len <= 0 )
		return false;
	buf[len] = '\0';
	executable_path.assign( buf );
	return true;
#elif defined(__APPLE__)
	char buf[4096];
	uint32_t sz = sizeof( buf );
	if( _NSGetExecutablePath( buf, &sz ) != 0 )
		return false;
	executable_path.assign( buf );
	return true;
#else
	(void)executable_path;
	return false;
#endif
}

CefMainArgs Platform_CreateMainArgs()
{
#if defined(_WIN32)
	return CefMainArgs( GetModuleHandleW( nullptr ) );
#else
	static char arg0[] = "xash3d";
	static char* argv[] = { arg0, nullptr };
	return CefMainArgs( 1, argv );
#endif
}

bool Platform_LoadCEF( const CefEnvironment& env )
{
#if defined(_WIN32)
	// libcef.dll is loaded by its full path. LOAD_WITH_ALTERED_SEARCH_PATH makes Windows look for the
	// libraries it depends on (chrome_elf.dll and others) in its own folder. The DLL search of the whole
	// game process is not changed: the engine and other libraries keep loading as before.
	std::filesystem::path libcef_path = PathFromUtf8( env.cefDir ) / "libcef.dll";
	libcef_path.make_preferred(); // LOAD_WITH_ALTERED_SEARCH_PATH needs backslashes
	const std::wstring libcef_wide = libcef_path.wstring();

	HMODULE module = LoadLibraryExW( libcef_wide.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH );

	if( !module )
	{
		Chrome_Printf(
			"[chrome] LoadLibrary(libcef.dll) failed %s (err=%lu)\n",
			PathToUtf8( libcef_path ).c_str(),
			static_cast<unsigned long>( GetLastError() ) );

		return false;
	}

	Chrome_DPrintf( 1.f, "[chrome] loaded %s\n", PathToUtf8( libcef_path ).c_str() );
	return true;

#elif defined(__linux__)
	const std::string libcef_path = PathToUtf8( PathFromUtf8( env.cefDir ) / "libcef.so" );
	if( !dlopen( libcef_path.c_str(), RTLD_NOW | RTLD_GLOBAL ) )
	{
		Chrome_Printf( "[chrome] dlopen failed %s: %s\n", libcef_path.c_str(), dlerror() );
		return false;
	}

	Chrome_DPrintf( 1.f, "[chrome] loaded %s\n", libcef_path.c_str() );
	return true;
#elif defined(__APPLE__)
	const std::string framework_binary = PathToUtf8( PathFromUtf8( env.frameworkDir ) / "Chromium Embedded Framework" );
	if( !cef_load_library( framework_binary.c_str() ) )
	{
		Chrome_Printf( "[chrome] cef_load_library failed: %s\n", framework_binary.c_str() );
		return false;
	}
	return true;
#else
	(void)env;
	return false;
#endif
}

} // namespace CEF
} // namespace iHTMLChrome

#endif
