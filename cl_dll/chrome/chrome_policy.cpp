#include "chrome_policy.h"

#if defined(CS16CLIENT_ENABLE_CEF)

#include "cef_command_line.h"

namespace iHTMLChrome {
namespace CEF {

namespace
{
bool g_single_process = true;
}

void SetCefSingleProcess( bool enable )
{
	g_single_process = enable;
}

void ApplyCefCommandLinePolicy( CefRefPtr<CefCommandLine> command_line )
{
	if( !command_line )
		return;

	command_line->AppendSwitch( "disable-gpu" );
	command_line->AppendSwitch( "disable-gpu-compositing" );
	command_line->AppendSwitch( "disable-gpu-vsync" );
	command_line->AppendSwitch( "disable-gpu-shader-disk-cache" );
	command_line->AppendSwitch( "disable-surfaces" );
	command_line->AppendSwitch( "disable-software-rasterizer" );
	command_line->AppendSwitch( "disable-accelerated-video-decode" );
	command_line->AppendSwitch( "enable-begin-frame-scheduling" );
	command_line->AppendSwitch( "disable-media-stream" );
	command_line->AppendSwitch( "disable-webrtc" );
	command_line->AppendSwitch( "disable-speech-api" );
	command_line->AppendSwitchWithValue(
		"disable-features",
		"MediaStream,MojoVideoCapture,WebRtcHideLocalIpsWithMdns" );
	command_line->AppendSwitch( "disable-extensions" );
	command_line->AppendSwitch( "disable-sync" );
	command_line->AppendSwitch( "disable-background-networking" );
	command_line->AppendSwitchWithValue( "autoplay-policy", "user-gesture-required" );
	command_line->AppendSwitch( "disable-video-capture-use-gpu-memory-buffer" );
	command_line->AppendSwitch( "enable-zero-copy" );
	command_line->AppendSwitch( "disable-partial-raster" );
	command_line->AppendSwitchWithValue( "js-flags", "--max-old-space-size=64" );
	command_line->AppendSwitch( "disable-javascript-harmony-shipping" );
	command_line->AppendSwitch( "disable-plugins" );
	command_line->AppendSwitch( "disable-translate" );
	command_line->AppendSwitch( "metrics-recording-only" );
	command_line->AppendSwitch( "enable-low-end-device-mode" );
	if( g_single_process )
		command_line->AppendSwitch( "single-process" );
}

} // namespace CEF
} // namespace iHTMLChrome

#endif
