#include "chrome_host.h"
#include "chrome_backend.h"
#include "chrome_debug.h"
#include "chrome_instance.h"
#include "chrome_motd_input.h"
#include "chrome_motd_utf.h"
#include "chrome_runtime.h"
#include "motd_layout.h"
#include "../hud/spectator_layout.h"
#include "../hud/motd_util.h"
#include "../hud/client_texture.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "hud.h"
#include "cl_util.h"
#include "draw_util.h"
#include "render_api.h"
#include "triangleapi.h"

namespace
{
using namespace iHTMLChrome::CEF;

std::unordered_map<int, std::unique_ptr<ChromeInstance>> g_instances;
int g_next_instance_id = 1;
bool g_chrome_service_up = false;
bool g_chrome_init_failed = false; // CefInitialize failed once: do not try again every MOTD

cvar_t* cl_chrome_zoom = nullptr;
cvar_t* cl_chrome_framerate = nullptr;

int g_motd_id = -1;
std::chrono::steady_clock::time_point g_motd_shown_at;
MotdInput g_motd_input;

// Text of the MOTD window in a font of Windows, from the atlas of the font (see hud/glyph_atlas.h)
struct MotdTextSlot
{
	CTextTexture text;
	int w = 0, ascent = 0;
};

MotdTextSlot g_motd_title, g_motd_ok;
std::unordered_map<int, std::pair<int, int>> g_last_mouse_pos; // for the generic SendMouse* functions

float CvarFloat( cvar_t* v, float fb )
{
	return v ? v->value : fb;
}

ChromeInstance* Lookup( int instance_id )
{
	const auto it = g_instances.find( instance_id );
	if( it == g_instances.end() || !it->second )
		return nullptr;
	return it->second.get();
}

void EraseInstanceRecord( int instance_id )
{
	const auto it = g_instances.find( instance_id );
	if( it == g_instances.end() )
		return;

	Chrome_DPrintf( 1.f, "[chrome] Destroy instance id=%d\n", instance_id );
	g_last_mouse_pos.erase( instance_id );
	g_instances.erase( it );
}

void RegisterEngineCvars()
{
	Chrome_DInit();

	if( !cl_chrome_zoom )
		cl_chrome_zoom = CVAR_CREATE( "cl_chrome_zoom", "1", FCVAR_ARCHIVE );
	if( !cl_chrome_framerate )
		cl_chrome_framerate = CVAR_CREATE( "cl_chrome_framerate", "30", FCVAR_ARCHIVE );
}

bool EnsureCefInitialized()
{
	if( g_chrome_service_up )
		return true;

	if( g_chrome_init_failed )
		return false;

	// CEF is started when it is needed for the first time (the first MOTD in mode 2),
	// not when the game starts: with cl_motd_browser 0 or 1 it is never started at all
	if( !iHTMLChrome::CEF::CefRuntime::Get().Initialize() )
	{
		g_chrome_init_failed = true;
		Chrome_Printf( "[chrome] CefRuntime::Initialize failed — browser features disabled\n" );
		return false;
	}

	g_chrome_service_up = true;
	Chrome_DPrintf( 1.f, "[chrome] service started (CefInitialize ok)\n" );
	return true;
}

// cl_chrome_openurl <address>: a page in the same window as the server MOTD (OK / Enter / cl_motd_close
// close it). A server can run this command too, but it can show its MOTD anyway, so it gets nothing new.
void Cmd_ChromeOpenUrl()
{
	std::string url;
	const int argc = gEngfuncs.Cmd_Argc();
	for( int i = 1; i < argc; ++i )
	{
		if( i > 1 )
			url += ' ';
		url += gEngfuncs.Cmd_Argv( i );
	}

	while( !url.empty() && ( url.front() == ' ' || url.front() == '\t' ) )
		url.erase( url.begin() );
	while( !url.empty() && ( url.back() == ' ' || url.back() == '\t' ) )
		url.pop_back();

	char safe[4096];
	if( url.empty() || !MotdUtil::MakeSafeUrl( url.c_str(), safe, sizeof( safe ) ) )
	{
		Chrome_Printf( "usage: cl_chrome_openurl <http:// or https:// address>\n" );
		return;
	}

	if( !Chrome::ShowMotd( safe ) )
		Chrome_Printf( "[chrome] the embedded browser is not available\n" );
}

void Cmd_MotdClose();

void RegisterCommands()
{
	static bool registered = false;
	if( registered || !gEngfuncs.pfnAddCommand )
		return;
	registered = true;
	gEngfuncs.pfnAddCommand( "cl_chrome_openurl", Cmd_ChromeOpenUrl );
	gEngfuncs.pfnAddCommand( "cl_motd_close", Cmd_MotdClose );
}

struct RenderExtents
{
	int w = 0;
	int h = 0;
};

RenderExtents VisibleRenderExtents()
{
	RenderExtents e;
	e.w = ScreenWidth;
	e.h = ScreenHeight;

	if( gRenderAPI.RenderGetParm )
	{
		const int rw = static_cast<int>( gRenderAPI.RenderGetParm( PARM_SCREEN_WIDTH, 0 ) );
		const int rh = static_cast<int>( gRenderAPI.RenderGetParm( PARM_SCREEN_HEIGHT, 0 ) );
		if( rw > 0 )
			e.w = rw;
		if( rh > 0 )
			e.h = rh;
	}

	return e;
}

double NowSeconds()
{
	return std::chrono::duration<double>( std::chrono::steady_clock::now().time_since_epoch() ).count();
}

// Executes what the input logic asks for
void ApplyMotdOutput( const MotdOutput& out )
{
	if( g_motd_id < 0 )
		return;

	if( ChromeInstance* inst = Lookup( g_motd_id ))
		for( int i = 0; i < out.count; ++i )
			inst->EnqueueInput( out.events[i] );

	if( out.close )
	{
		gEngfuncs.Con_Printf( "[motd] window closed by user\n" );
		Chrome::HideMotd();
	}
}

void DrawMotdCursor( int x, int y )
{
	// an arrow made of 1 pixel rows: black outline, white inside
	for( int r = 0; r < 14; ++r )
	{
		const int w = r < 9 ? r + 1 : ( r < 12 ? 4 : 3 );
		FillRGBA( x - 1, y + r, w + 2, 1, 0, 0, 0, 255 );
	}
	FillRGBA( x - 1, y - 1, 3, 1, 0, 0, 0, 255 );
	FillRGBA( x - 1, y + 14, 5, 1, 0, 0, 0, 255 );

	for( int r = 0; r < 14; ++r )
	{
		const int w = r < 9 ? r + 1 : ( r < 12 ? 4 : 3 );
		FillRGBA( x, y + r, w, 1, 255, 255, 255, 255 );
	}
}

void FillFrame( int x, int y, int w, int h, int r, int g, int b, int a )
{
	FillRGBABlend( x, y, w, 1, r, g, b, a );
	FillRGBABlend( x, y + h - 1, w, 1, r, g, b, a );
	FillRGBABlend( x, y + 1, 1, h - 2, r, g, b, a );
	FillRGBABlend( x + w - 1, y + 1, 1, h - 2, r, g, b, a );
}

void FreeMotdText( MotdTextSlot& slot )
{
	slot.text.Free();
	slot.w = slot.ascent = 0;
}

// Lays the text out, or keeps it if nothing has changed (a line too wide ends with "...").
// Returns false if the text cannot be drawn this way (then the font of the HUD is used).
bool UpdateMotdText( MotdTextSlot& slot, const std::string& utf8, const char* face, int pixelHeight, int weight, bool antialias, int maxWidth )
{
	if( !slot.text.Update( utf8, face, pixelHeight, weight, antialias, maxWidth > 0 ? maxWidth : 4096, true ) || !slot.text.Valid() )
		return false;

	slot.w = slot.text.Width();
	slot.ascent = slot.text.Ascent();
	return true;
}

// The text with its baseline at (x, baseline); every texel of the atlas lands on its own pixel
void DrawMotdText( const MotdTextSlot& slot, int x, int baseline, int r, int g, int b )
{
	slot.text.Draw( (float)x, (float)( baseline - slot.ascent ), r, g, b );
}

// The CS icon of the title: gfx/vgui/CS_logo.tga of the game (a white figure, painted with "TitleIcon"
// of ClientScheme.res, 255 174 0, as in the Steam version). Nothing is drawn if the game has no such file.
int g_motd_icon = 0;
bool g_motd_icon_tried = false;

void DrawMotdIcon( int x, int y, int size )
{
	if( !g_motd_icon_tried && gRenderAPI.GL_LoadTexture && gEngfuncs.COM_LoadFile )
	{
		g_motd_icon_tried = true;

		static const char *const names[] =
		{
			"gfx/vgui/CS_logo.tga",
			"gfx/vgui/cs_logo.tga",
		};

		for( const char *name : names )
		{
			int len = 0;
			byte *probe = gEngfuncs.COM_LoadFile( name, 5, &len );

			if( !probe )
				continue;

			gEngfuncs.COM_FreeFile( probe );
			g_motd_icon = gRenderAPI.GL_LoadTexture( name, NULL, 0, TF_NOMIPMAP | TF_CLAMP );

			if( g_motd_icon > 0 )
			{
				gEngfuncs.Con_DPrintf( "[motd] icon: %s\n", name );
				break;
			}
		}
	}

	if( g_motd_icon <= 0 || size <= 0 )
		return;

	gRenderAPI.GL_SelectTexture( 0 );
	gRenderAPI.GL_Bind( 0, g_motd_icon );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 174 / 255.0f, 0.0f, 1.0f );
	DrawUtils::Draw2DQuad( (float)x, (float)y, (float)( x + size ), (float)( y + size ));
}

// The server MOTD is shown as a window like the one of the Steam version of the game:
// a dark rounded panel with a cut through it, the server name, the page and the OK button.
// All sizes are measured on the window of the Steam version at 1920x1080 and grow with the screen
// height like in the Steam version (see motd_layout.h).
void PresentMotdWindow( ChromeInstance* inst )
{
	if( !inst )
		return;

	const unsigned int tex = inst->texture_id();
	if( tex == 0 || !gEngfuncs.pTriAPI )
		return;

	const RenderExtents ext = VisibleRenderExtents();
	if( ext.w <= 1 || ext.h <= 1 )
		return;

	int bw = inst->width();
	int bh = inst->height();
	if( bw < 1 )
		bw = 1;
	if( bh < 1 )
		bh = 1;

	inst->set_viewport( ChromeRect{ 0, 0, bw, bh } );

	// the window of the Steam version, sized in units of 1/480 of the screen height (see motd_layout.h)
	const MotdWin::Layout L = MotdWin::Compute( ext.w, ext.h );

	// the browser draws the page pixel for pixel: it gets the size the page has on the screen
	if( bw != L.pageW || bh != L.pageH )
		inst->Resize( L.pageW, L.pageH );

	MotdLayout lay;
	lay.valid = true;
	lay.x = (float)L.pageX;
	lay.y = (float)L.pageY;
	lay.w = (float)L.pageW;
	lay.h = (float)L.pageH;
	lay.scale = (float)L.pageW / bw;
	lay.close_x = (float)L.btnX;
	lay.close_y = (float)L.btnY;
	lay.close_w = (float)L.btnW;
	lay.close_h = (float)L.btnH;
	g_motd_input.SetScreen( ext.w, ext.h );
	g_motd_input.SetLayout( lay );

	// panel: two parts with a cut between them ("ViewportBG" of the scheme: black, alpha 200)
	const int upperH = L.cutY - L.panelY;
	const int lowerY = L.cutY + L.cutH;
	DrawUtils::FillRoundedRect( L.panelX, L.panelY, L.panelW, upperH, L.radius, true, false, 0, 0, 0, 204 );
	DrawUtils::FillRoundedRect( L.panelX, lowerY, L.panelW, L.panelY + L.panelH - lowerY, L.radius, false, true, 0, 0, 0, 204 );

	// the CS icon on the left of the title (a picture of the game, if it has one)
	DrawMotdIcon( L.iconX, L.iconY, L.iconSize );

	// title: the name of the server
	const std::string title = ToUtf8( gHUD.m_szServerName[0] ? gHUD.m_szServerName : "MOTD" );

	if( UpdateMotdText( g_motd_title, title, "Trebuchet MS", std::max( 8, L.titleEm ), 400, true, L.titleMaxW ))
	{
		DrawMotdText( g_motd_title, L.titleX, L.titleBaseline, 255, 176, 0 );
	}
	else
	{
		// no Windows fonts: the font of the HUD
		const float titleScale = 1.4f * std::max( 0.6f, L.u / 1.875f );
		const int titleY = L.titleBaseline - (int)( gHUD.GetCharHeight() * titleScale );
		DrawUtils::DrawHudString( L.titleX, titleY, 9999, title.c_str(), 255, 176, 0, titleScale );
	}

	// the page
	gRenderAPI.GL_SelectTexture( 0 );
	gRenderAPI.GL_Bind( 0, tex );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	DrawUtils::Draw2DQuad( (float)L.pageX, (float)L.pageY, (float)( L.pageX + L.pageW ), (float)( L.pageY + L.pageH ));

	// OK button
	const bool hover = g_motd_input.cursor_over_close();
	FillRGBABlend( L.btnX, L.btnY, L.btnW, L.btnH, 0, 0, 0, hover ? 230 : 200 );
	if( hover )
		FillFrame( L.btnX, L.btnY, L.btnW, L.btnH, 214, 147, 0, 255 );
	else
		FillFrame( L.btnX, L.btnY, L.btnW, L.btnH, 84, 58, 0, 255 );

	// "OK" in the font "Default" of resource/ClientScheme.res (Verdana, weight 600, its size depends on the screen)
	const SpecBar::Font okFont = SpecBar::FontFor( ext.h );
	const int okEm = SpecBar::FontEm( okFont.tall );

	if( UpdateMotdText( g_motd_ok, "OK", "Verdana", okEm, 600, okFont.antialias, 0 ))
	{
		DrawMotdText( g_motd_ok, L.okCenterX - g_motd_ok.w / 2, L.okBaseline, 255, 176, 0 );
	}
	else
	{
		const float okScale = 0.85f * std::max( 0.6f, L.u / 1.875f );
		const int fbW = (int)( DrawUtils::HudStringLen( "OK" ) * okScale );
		const int fbY = L.btnY + ( L.btnH - (int)( gHUD.GetCharHeight() * okScale )) / 2;
		DrawUtils::DrawHudString( L.okCenterX - fbW / 2, fbY, 9999, "OK", 255, 176, 0, okScale );
	}

	DrawMotdCursor( (int)g_motd_input.cursor_x(), (int)g_motd_input.cursor_y() );
}

void Cmd_MotdClose()
{
	if( Chrome::IsMotdShown() )
	{
		gEngfuncs.Con_Printf( "[motd] closed by command\n" );
		Chrome::HideMotd();
	}
}

} // namespace

namespace Chrome {

bool Initialize()
{
	// only the variables and commands: CEF itself is started by the first MOTD that needs it
	RegisterEngineCvars();
	RegisterCommands();
	return true;
}

void Shutdown()
{
	FreeMotdText( g_motd_title );
	FreeMotdText( g_motd_ok );
	g_instances.clear(); // asks every browser to close
	g_motd_id = -1;

	if( !g_chrome_service_up )
		return;

	// CefShutdown must not be called while a browser is still open (the game may crash or hang on exit):
	// let CEF finish closing them first
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );

	while( LiveBrowserCount() > 0 && std::chrono::steady_clock::now() < deadline )
	{
		iHTMLChrome::CEF::CefRuntime::Get().DoMessageLoopWork();
		std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}

	if( LiveBrowserCount() > 0 )
	{
		// shutting CEF down now would be worse than leaving it to the end of the process
		Chrome_Printf( "[chrome] %d browser(s) did not close in time, CEF shutdown is skipped\n", LiveBrowserCount() );
		g_chrome_service_up = false;
		return;
	}

	iHTMLChrome::CEF::CefRuntime::Get().Shutdown();
	g_chrome_service_up = false;
	Chrome_DPrintf( 1.f, "[chrome] Chrome::Shutdown complete\n" );
}

void RunFrame()
{
	if( !g_chrome_service_up )
		return;

	SetChromeZoom( static_cast<double>( CvarFloat( cl_chrome_zoom, 1.0f ) ) );
	SetChromeFrameRate( static_cast<int>( CvarFloat( cl_chrome_framerate, 30.0f ) ) );

	iHTMLChrome::CEF::CefRuntime::Get().DoMessageLoopWork();

	for( auto& kv : g_instances )
		if( kv.second )
			kv.second->Tick();
}

void PresentMotd()
{
	if( !g_chrome_service_up )
		return;

	if( g_motd_id >= 0 )
		PresentMotdWindow( Lookup( g_motd_id ) );
}

int Create(
	const char* url,
	const int width,
	const int height,
	const ChromeOnPaintCallback callback,
	void* userdata )
{
	if( !EnsureCefInitialized() )
		return -1;

	const int logical_w = width > 0 ? width : 1280;
	const int logical_h = height > 0 ? height : 720;

	const int id = g_next_instance_id++;

	auto inst = std::make_unique<ChromeInstance>( id );
	const char* start = ( url && url[0] ) ? url : "about:blank";

	Chrome_DPrintf(
		1.f,
		"[chrome] Create id=%d url=%s logical=%dx%d\n",
		id,
		start,
		logical_w,
		logical_h );

	if( !inst->Initialize( start ) )
	{
		Chrome_Printf( "[chrome] Chrome::Create failed during Initialize\n" );
		return -1;
	}

	if( callback )
		inst->SetPaintConsumer(
			reinterpret_cast<ChromePaintConsumerFn>( callback ), userdata );

	g_instances.emplace( id, std::move( inst ) );

	Chrome::Resize( id, logical_w, logical_h );
	return id;
}

void Destroy( const int instance_id )
{
	if( instance_id == g_motd_id )
		g_motd_id = -1;
	EraseInstanceRecord( instance_id );
}

void Resize( const int instance_id, const int width, const int height )
{
	ChromeInstance* inst = Lookup( instance_id );
	if( !inst )
		return;

	Chrome_DPrintf( 1.f, "[chrome] Resize id=%d -> %dx%d\n", instance_id, width, height );
	inst->Resize( width, height );
}

void Navigate( const int instance_id, const char* url )
{
	ChromeInstance* inst = Lookup( instance_id );
	if( !inst )
		return;

	const char* nav = url && url[0] ? url : "about:blank";
	Chrome_DPrintf( 1.f, "[chrome] Navigate id=%d -> %s\n", instance_id, nav );
	inst->LoadURL( std::string( nav ) );
}

void SendMouseMove( const int instance_id, const int x, const int y )
{
	ChromeInstance* inst = Lookup( instance_id );
	if( !inst )
		return;

	g_last_mouse_pos[instance_id] = std::make_pair( x, y );

	InputEvent e;
	e.type = InputType::MouseMove;
	e.x = x;
	e.y = y;
	inst->EnqueueInput( e );
}

void SendMouseButton( const int instance_id, const int button, const bool down )
{
	ChromeInstance* inst = Lookup( instance_id );
	if( !inst )
		return;

	const auto pos = g_last_mouse_pos.find( instance_id );

	InputEvent e;
	e.type = down ? InputType::MouseDown : InputType::MouseUp;
	e.button = button;
	if( pos != g_last_mouse_pos.end() )
	{
		e.x = pos->second.first;
		e.y = pos->second.second;
	}
	inst->EnqueueInput( e );
}

void SendMouseWheel( const int instance_id, const int delta_x, const int delta_y )
{
	ChromeInstance* inst = Lookup( instance_id );
	if( !inst )
		return;

	const auto pos = g_last_mouse_pos.find( instance_id );

	InputEvent e;
	e.type = InputType::MouseWheel;
	e.delta_x = delta_x;
	e.delta_y = delta_y;
	if( pos != g_last_mouse_pos.end() )
	{
		e.x = pos->second.first;
		e.y = pos->second.second;
	}
	inst->EnqueueInput( e );
}

void SendKeyEvent( const int instance_id, const int key_code, const bool down )
{
	ChromeInstance* inst = Lookup( instance_id );
	if( !inst )
		return;

	int vk, ch;
	if( !MapMotdKey( key_code, false, vk, ch ))
		return;

	InputEvent e;
	e.type = down ? InputType::KeyDown : InputType::KeyUp;
	e.key = vk;
	inst->EnqueueInput( e );

	if( down && ch )
	{
		InputEvent c;
		c.type = InputType::KeyChar;
		c.key = ch;
		c.codepoint = (std::uint32_t)ch;
		inst->EnqueueInput( c );
	}
}

unsigned int GetTextureId( const int instance_id )
{
	ChromeInstance* inst = Lookup( instance_id );
	if( !inst )
		return 0;
	return inst->texture_id();
}

bool IsAvailable()
{
#if defined( CS16CLIENT_ENABLE_CEF )
	return !g_chrome_init_failed; // built in, and did not fail to start (it is started on demand)
#else
	return false;
#endif
}

bool ShowMotd( const char* url )
{
	if( !IsAvailable() || !url || !url[0] || !EnsureCefInitialized() )
		return false;

	if( ChromeInstance* inst = Lookup( g_motd_id ))
	{
		// the window is already open (the server sent its MOTD again): the same browser shows the new page,
		// no new browser is created for every message
		inst->LoadURL( std::string( url ) );
	}
	else
	{
		HideMotd();

		// the page gets the size of the page of the Steam version (480 x 240 units, 1080 x 540 at 1080)
		const RenderExtents ext0 = VisibleRenderExtents();
		const MotdWin::Layout L0 = MotdWin::Compute( ext0.w > 1 ? ext0.w : 1920, ext0.h > 1 ? ext0.h : 1080 );
		const int id = Chrome::Create( url, L0.pageW, L0.pageH, nullptr, nullptr );
		if( id < 0 )
			return false;

		g_motd_id = id;
	}

	FreeMotdText( g_motd_title );
	FreeMotdText( g_motd_ok );
	g_motd_shown_at = std::chrono::steady_clock::now();

	const RenderExtents ext = VisibleRenderExtents();
	g_motd_input.Open( ext.w, ext.h );

	gEngfuncs.Con_Printf( "[motd] shown: mouse and keyboard work in the window, ENTER / OK button / cl_motd_close close it\n" );
	return true;
}

void HideMotd()
{
	if( g_motd_id < 0 )
		return;

	const int id = g_motd_id;
	g_motd_id = -1;
	Chrome::Destroy( id );
	FreeMotdText( g_motd_title );
	FreeMotdText( g_motd_ok );
	gEngfuncs.Con_Printf( "[motd] window closed\n" );
}

bool IsMotdShown()
{
	return g_motd_id >= 0;
}

bool HandleMotdKey( int down, int keynum )
{
	if( keynum < 0 || keynum >= 256 )
		return false;

	// the window is closed: the release of a key that was used by it must not reach the game either
	if( g_motd_id < 0 )
		return !down && g_motd_input.OnKey( 0, keynum, NowSeconds() ).consumed;

	const MotdOutput out = g_motd_input.OnKey( down, keynum, NowSeconds() );

	ApplyMotdOutput( out );
	return out.consumed;
}

bool MotdCapturesMouse()
{
	return g_motd_id >= 0;
}

void MotdMouseDelta( float dx, float dy )
{
	if( g_motd_id >= 0 )
		ApplyMotdOutput( g_motd_input.OnMouseDelta( dx, dy ));
}

void HideMotdOnLevelChange()
{
	// the MOTD of a server comes right after the level is loaded, do not close it by mistake
	if( g_motd_id >= 0 && std::chrono::steady_clock::now() - g_motd_shown_at > std::chrono::seconds( 3 ))
	{
		gEngfuncs.Con_Printf( "[motd] window closed: a new level was loaded\n" );
		HideMotd();
	}
}
} // namespace Chrome
