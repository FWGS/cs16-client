#include "chrome_runtime.h" // also needed without CEF, see the end of the file

#if defined(CS16CLIENT_ENABLE_CEF)

#include "chrome_runtime.h"
#include "chrome_policy.h"
#include "chrome_environment.h"
#include "chrome_installation.h"
#include "chrome_platform.h"
#include "chrome_settings.h"
#include "chrome_debug.h"
#include "chrome_path.h"

#include "cef_app.h"
#include "cef_command_line.h"

#include <filesystem>

namespace
{
class BrowserCefApp final : public CefApp, public CefBrowserProcessHandler
{
public:
	CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

	void OnBeforeCommandLineProcessing( const CefString&, CefRefPtr<CefCommandLine> command_line ) override
	{
		iHTMLChrome::CEF::ApplyCefCommandLinePolicy( command_line );
	}

	IMPLEMENT_REFCOUNTING( BrowserCefApp );
};
} // namespace

namespace iHTMLChrome {
namespace CEF {

CefRuntime& CefRuntime::Get()
{
	static CefRuntime runtime;
	return runtime;
}

bool CefRuntime::Initialize()
{
	if( initialized_ )
		return true;

	Chrome_DInit();

	CefEnvironment env;
	if( !BuildCefEnvironment( env ) )
		return false;

	if( !Platform_GetExecutablePath( env.subprocessPath ) || env.subprocessPath.empty() )
	{
		Chrome_Printf( "[chrome] CefRuntime: executable path unresolved\n" );
		return false;
	}

	if( !CefInstallation_Validate( env ) )
		return false;

#if defined( _WIN32 )
	// Normal multi-process mode: a separate helper executable runs the renderer and the other
	// Chromium processes. Without it Chromium has to run inside the game process (single-process).
	{
		const std::filesystem::path helper = PathFromUtf8( env.cefDir ) / "cs16_cef_helper.exe";
		std::error_code ec;

		if( std::filesystem::exists( helper, ec ))
		{
			env.subprocessPath = PathToUtf8( helper );
			SetCefSingleProcess( false );
			Chrome_Printf( "[chrome] multi-process mode, helper: %s\n", env.subprocessPath.c_str() );
		}
		else
		{
			SetCefSingleProcess( true );
			Chrome_Printf( "[chrome] single-process mode (no cs16_cef_helper.exe in %s)\n", env.cefDir.c_str() );
		}
	}
#endif

	if( !CefInstallation_EnsureCachePath( env ) )
		return false;

	if( !Platform_LoadCEF( env ) )
		return false;

	const CefMainArgs main_args = Platform_CreateMainArgs();
	const CefSettings settings = BuildCefSettings( env );

	CefRefPtr<BrowserCefApp> app( new BrowserCefApp() );
	if( !CefInitialize( main_args, settings, app, nullptr ) )
	{
		Chrome_Printf( "[chrome] CefInitialize failed\n" );
		return false;
	}

	initialized_ = true;
	Chrome_DPrintf( 1.f, "[chrome] CefRuntime initialized\n" );
	return true;
}

void CefRuntime::DoMessageLoopWork()
{
	if( initialized_ )
		CefDoMessageLoopWork();
}

void CefRuntime::Shutdown()
{
	if( !initialized_ )
		return;

	CefShutdown();
	initialized_ = false;

	Chrome_DPrintf( 1.f, "[chrome] CefRuntime shutdown complete\n" );
}
} // namespace CEF
} // namespace iHTMLChrome

#else // !CS16CLIENT_ENABLE_CEF

namespace iHTMLChrome {
namespace CEF {

CefRuntime& CefRuntime::Get()
{
	static CefRuntime runtime;
	return runtime;
}

bool CefRuntime::Initialize()
{
	initialized_ = true;
	return true;
}

void CefRuntime::DoMessageLoopWork()
{
}

void CefRuntime::Shutdown()
{
	initialized_ = false;
}
} // namespace CEF
} // namespace iHTMLChrome

#endif
