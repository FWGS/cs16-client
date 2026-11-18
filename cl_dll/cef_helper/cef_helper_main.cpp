// Helper executable of the embedded browser (CEF).
//
// Chromium starts its renderer and other service processes from this executable instead of
// the game, so the web pages of a server are not executed inside the game process.
// It lives in <gamedir>/cef next to libcef.dll.

#if defined( CS16CLIENT_ENABLE_CEF ) && defined( _WIN32 )

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "cef_app.h"
#include "cef_command_line.h"

#include "chrome_policy.h"

namespace
{
// Same command line switches as in the game process (it is called in every Chromium process)
class HelperApp final : public CefApp
{
public:
	void OnBeforeCommandLineProcessing( const CefString&, CefRefPtr<CefCommandLine> command_line ) override
	{
		iHTMLChrome::CEF::SetCefSingleProcess( false );
		iHTMLChrome::CEF::ApplyCefCommandLinePolicy( command_line );
	}

	IMPLEMENT_REFCOUNTING( HelperApp );
};
} // namespace

int APIENTRY wWinMain( HINSTANCE hInstance, HINSTANCE, LPWSTR, int )
{
	CefMainArgs main_args( hInstance );
	CefRefPtr<HelperApp> app( new HelperApp() );

	const int exit_code = CefExecuteProcess( main_args, app, nullptr );

	// a negative value: started by hand, not as a Chromium process
	return exit_code >= 0 ? exit_code : 0;
}

#endif
