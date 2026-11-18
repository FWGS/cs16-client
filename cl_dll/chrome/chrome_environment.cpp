#include "chrome_environment.h"

#if defined(CS16CLIENT_ENABLE_CEF)

#include "chrome_debug.h"
#include "chrome_path.h"

#include <filesystem>

#include "hud.h"

namespace iHTMLChrome {
namespace CEF {
namespace {

std::string ExpandPath( const std::string& path )
{
	if( path.empty() || !gEngfuncs.COM_ExpandFilename )
		return path;

	char out[4096];
	out[0] = '\0';
	if( gEngfuncs.COM_ExpandFilename( path.c_str(), out, sizeof( out ) ) == 1 && out[0] != '\0' )
		return std::string( out );

	return path;
}

std::string NormalizePath( const std::filesystem::path& path )
{
	return PathToUtf8( path.lexically_normal() );
}

std::filesystem::path ToAbsolutePath( const std::filesystem::path& path )
{
	std::error_code ec;
	const std::filesystem::path absolute_path = std::filesystem::absolute( path, ec );
	if( !ec && !absolute_path.empty() )
		return absolute_path.lexically_normal();

	return path.lexically_normal();
}
} // namespace

bool BuildCefEnvironment( CefEnvironment& env )
{
	const char* game_dir = gEngfuncs.pfnGetGameDirectory ? gEngfuncs.pfnGetGameDirectory() : nullptr;
	if( !game_dir || !game_dir[0] )
	{
		Chrome_Printf( "[chrome] CefRuntime: game directory is unavailable\n" );
		return false;
	}

	std::filesystem::path game_path = ToAbsolutePath( PathFromEngine( ExpandPath( game_dir ) ) );
	if( game_path.empty() )
	{
		Chrome_Printf( "[chrome] CefRuntime: failed to resolve game directory\n" );
		return false;
	}

	env = CefEnvironment{};
	env.gameDir = NormalizePath( game_path );
	env.cefDir = NormalizePath( game_path / "cef" );
	env.cachePath = NormalizePath( game_path / "cef" / "cache" );
	env.resourcesDir = env.cefDir;

#if defined(__APPLE__)
	env.frameworkDir = NormalizePath( game_path / "Chromium Embedded Framework.framework" );
	env.resourcesDir = NormalizePath( PathFromUtf8( env.frameworkDir ) / "Resources" );
#endif

	return true;
}

} // namespace CEF
} // namespace iHTMLChrome

#endif
