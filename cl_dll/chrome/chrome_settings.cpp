#include "chrome_settings.h"

#if defined(CS16CLIENT_ENABLE_CEF)

#include "chrome_environment.h"
#include "chrome_debug.h"
#include "chrome_path.h"

#include <filesystem>

namespace iHTMLChrome {
namespace CEF {
namespace {

std::string ResolveResourcesDir( const CefEnvironment& env )
{
#if defined(_WIN32) || defined(__linux__)
	const std::filesystem::path resources_root = PathFromUtf8( env.resourcesDir );
	std::error_code ec;
	if( std::filesystem::exists( resources_root / "icudtl.dat", ec ) )
		return env.resourcesDir;

	const std::filesystem::path nested_resources = PathFromUtf8( env.cefDir ) / "Resources";
	ec.clear();
	if( std::filesystem::exists( nested_resources / "icudtl.dat", ec ) )
		return PathToUtf8( nested_resources );

	return env.resourcesDir;
#else
	return env.resourcesDir;
#endif
}

std::string FindLocalesDir( const std::string& resources_dir )
{
	const std::filesystem::path direct = PathFromUtf8( resources_dir ) / "locales";
	std::error_code ec;
	if( std::filesystem::exists( direct, ec ) )
		return PathToUtf8( direct );

	const std::filesystem::path nested = PathFromUtf8( resources_dir ) / "Resources" / "locales";
	ec.clear();
	if( std::filesystem::exists( nested, ec ) )
		return PathToUtf8( nested );

	return PathToUtf8( direct );
}
} // namespace

CefSettings BuildCefSettings( const CefEnvironment& env )
{
	CefSettings settings;
	// The sandbox of Chromium is off: on Windows it needs the executable of the application itself (xash3d.exe)
	// linked with cef_sandbox and starting CEF, which a client DLL cannot do. The pages are limited instead
	// (chrome_backend_cef.cpp: http(s), about:blank and data:text/html only, no downloads, no popup windows) and
	// run in the helper process (cs16_cef_helper), not in the game.
	settings.no_sandbox = true;
	settings.windowless_rendering_enabled = true;
	settings.external_message_pump = false;
	settings.multi_threaded_message_loop = false;
	settings.log_severity = Chrome_Level() >= 1.f ? LOGSEVERITY_VERBOSE : LOGSEVERITY_ERROR;
	settings.persist_session_cookies = false;

	const std::string resources_dir = ResolveResourcesDir( env );

	CefString( &settings.browser_subprocess_path ) = env.subprocessPath;
	CefString( &settings.resources_dir_path ) = resources_dir;
	CefString( &settings.locales_dir_path ) = FindLocalesDir( resources_dir );
	CefString( &settings.root_cache_path ) = env.cachePath;
	CefString( &settings.cache_path ) = env.cachePath;

#if defined(__APPLE__)
	CefString( &settings.framework_dir_path ) = env.frameworkDir;
#endif

	Chrome_DPrintf( 1.f, "[chrome] resources_dir=%s\n", resources_dir.c_str() );
	Chrome_DPrintf( 1.f, "[chrome] cache_path=%s\n", env.cachePath.c_str() );
	return settings;
}

} // namespace CEF
} // namespace iHTMLChrome

#endif
