/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// MOTD.cpp
//
// for displaying a server-sent message of the day
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include "draw_util.h"

#include "keydefs.h"
#ifdef _CSWEB_ENABLE
#include "csw_interface.h"
#endif

// ------------------------------------------------------------------ layout
#define MOTD_PAD          16
#define MOTD_BTN_W        120
#define MOTD_BTN_H        32
#define MOTD_BTN_GAP      24
#define MOTD_MARGIN       30
#define MOTD_TEXT_H       13
#define MOTD_SCR_W        40
#define MOTD_SCR_GAP      8
#define MOTD_SCROLL_SPEED 20.0f // lines per second

// Interpolation anchors: screen size -> browser size.
// The width is taken from ScreenWidth, the height from ScreenHeight.
struct MOTDPoint { int screen, size; };

static const MOTDPoint s_BrowserW[] = { { 640, 450 }, { 800, 590 }, { 1024, 760 }, { 1600, 960 } };
static const MOTDPoint s_BrowserH[] = { { 480, 400 }, { 600, 460 }, { 768, 600 }, { 1200, 850 } };

// ------------------------------------------------------------------ helpers
// Piecewise linear interpolation between the anchors. Below the first anchor
// the size is proportional, above the last one the last segment is extrapolated.
template<size_t N>
static int MOTDInterp( const MOTDPoint ( &t )[N], int v )
{
	if( v <= t[0].screen )
		return t[0].size * v / t[0].screen;

	size_t i = 0;
	while( i + 2 < N && v > t[i + 1].screen )
		i++;

	return t[i].size + ( v - t[i].screen ) * ( t[i + 1].size - t[i].size ) / ( t[i + 1].screen - t[i].screen );
}

static CUtlString MakeDataUrl( const CUtlString &html )
{
	CUtlString url( "data:text/html;charset=utf-8;base64," );
	url.AppendBase64( (const uint8_t *)html.String(), html.Length() );
	return url;
}

// Plain text is wrapped into an escaped <pre>.
static CUtlString TextToHtml( const char *text )
{
	CUtlString esc( text );
	esc.Replace( "&", "&amp;" ); // '&' must be replaced first
	esc.Replace( "<", "&lt;" );
	esc.Replace( ">", "&gt;" );

	CUtlString out(
		"<!DOCTYPE html><meta charset=\"utf-8\">"
		"<body style=\"margin:8px;background:#111;color:#ffb400;font:14px monospace\">"
		"<pre style=\"white-space:pre-wrap;margin:0\">" );
	out += esc;
	out += "</pre></body>";
	return out;
}

// Invisible client touch button; coordinates are normalized to the screen.
static void AddTouchButton( const char *name, const char *cmd, int x, int y, int w, int h )
{
	static byte color[4] = { 0, 0, 0, 0 };

	gMobileAPI.pfnTouchAddClientButton( name, "*white", cmd,
		(float)x / ScreenWidth,
		(float)y / ScreenHeight,
		(float)( x + w ) / ScreenWidth,
		(float)( y + h ) / ScreenHeight,
		color, 0, 1.0f, 0 );
}

#ifdef _CSWEB_ENABLE
// Page click for touch input. The cursor position is refreshed first,
// so the click lands where the finger is.
static void SendPageClick( bool down )
{
	int mx, my;
	gEngfuncs.GetMousePosition( &mx, &my );
	CSW_MouseMove( mx, my );
	CSW_MouseButton( 0, down );
}
#endif

// ------------------------------------------------------------------ console commands
void Close_f( void )
{
	gHUD.m_MOTD.Reset();
}

void OpenURL_f( void )
{
	if( gEngfuncs.Cmd_Argc() < 2 )
		return;

	gHUD.m_MOTD.CreateBrowser( gEngfuncs.Cmd_Argv( 1 ) );
}

// ------------------------------------------------------------------ lifecycle
int CHudMOTD :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( gHUD.m_MOTD, MOTD );

	cl_motd_hide = CVAR_CREATE( "cl_motd_hide", "0", FCVAR_ARCHIVE ); // hide motd
	gEngfuncs.pfnAddCommand( "cl_motd_openurl", OpenURL_f );
	gEngfuncs.pfnAddCommand( "cl_motd_close", Close_f );
	Reset();

	return 1;
}

int CHudMOTD :: VidInit( void )
{
	// Load sprites here
	return 1;
}

void CHudMOTD :: Reset( void )
{
	m_iFlags &= ~HUD_DRAW;
	m_szMOTD.Clear();
	m_bShow = false;
	m_iButtons = 0;
	m_iBrowserW = m_iBrowserH = 0;
	memset( &m_touchLayout, 0xFF, sizeof( m_touchLayout ) ); // never equal to a real layout

	if( g_iMobileAPIVersion )
	{
		gMobileAPI.pfnTouchRemoveButton( "_motd_*" );
		gMobileAPI.pfnTouchSetClientOnly( false );
	}

#ifdef _CSWEB_ENABLE
	CSW_Destroy();
#endif
}

// Creates the browser and opens the window. Any previous window is closed first.
bool CHudMOTD :: CreateBrowser( const char *url )
{
	Reset();

#ifdef _CSWEB_ENABLE
	Layout l;
	CalcLayout( l );

	if( !CSW_Create( l.bw, l.bh, url ) )
		return false;

	m_szUrl = url;
	m_iBrowserW = l.bw;
	m_iBrowserH = l.bh;
	m_iFlags |= HUD_DRAW;
	m_bShow = true;

	if( g_iMobileAPIVersion )
		gMobileAPI.pfnTouchSetClientOnly( true );
	UpdateTouch( l );

	return true;
#else
	return false;
#endif
}

// ------------------------------------------------------------------ layout
void CHudMOTD :: CalcLayout( Layout &l ) const
{
	const int chromeW = MOTD_PAD * 2;
	const int chromeH = MOTD_PAD * 2 + MOTD_BTN_GAP + MOTD_BTN_H;

	// browser size from the table, but never larger than what fits on the screen
	l.bw = MOTDInterp( s_BrowserW, ScreenWidth );
	l.bh = MOTDInterp( s_BrowserH, ScreenHeight );

	const int maxBw = ScreenWidth  - MOTD_MARGIN * 2 - chromeW;
	const int maxBh = ScreenHeight - MOTD_MARGIN * 2 - chromeH;
	if( l.bw > maxBw ) l.bw = maxBw;
	if( l.bh > maxBh ) l.bh = maxBh;
	if( l.bw < 1 ) l.bw = 1;
	if( l.bh < 1 ) l.bh = 1;

	// window frame around the browser, centered on the screen
	l.pw = l.bw + chromeW;
	l.ph = l.bh + chromeH;
	l.px = ( ScreenWidth  - l.pw ) / 2;
	l.py = ( ScreenHeight - l.ph ) / 2;

	l.bx = l.px + MOTD_PAD;
	l.by = l.py + MOTD_PAD;

	// OK button: bottom center
	l.btnX = l.px + ( l.pw - MOTD_BTN_W ) / 2;
	l.btnY = l.py + l.ph - MOTD_PAD - MOTD_BTN_H;

	// scroll buttons: right end of the bottom row
	l.scrDownX = l.px + l.pw - MOTD_PAD - MOTD_SCR_W;
	l.scrUpX   = l.scrDownX - MOTD_SCR_GAP - MOTD_SCR_W;
}

bool CHudMOTD :: IsOverButton( const Layout &l ) const
{
	int mx, my;
	gEngfuncs.GetMousePosition( &mx, &my );

	return mx >= l.btnX && mx < l.btnX + MOTD_BTN_W &&
	       my >= l.btnY && my < l.btnY + MOTD_BTN_H;
}

// ------------------------------------------------------------------ input
// Called from HUD_Key_Event: raw keys and the mouse (desktop).
// true = the key is handled by the window.
bool CHudMOTD :: KeyEvent( int down, int keynum )
{
	if( !m_bShow )
		return false;

	if( keynum == K_ESCAPE )
	{
		if( down )
			Reset();
		return true;
	}

	// click on OK: close on release
	if( keynum == K_MOUSE1 )
	{
		Layout l;
		CalcLayout( l );
		if( IsOverButton( l ) )
		{
			if( !down )
				Reset();
			return true;
		}
	}

#ifdef _CSWEB_ENABLE
	return CSW_Key( down, keynum );
#else
	return false;
#endif
}

// Called from the +forward / +back / +attack handlers (see input.cpp).
// On Android in client-only mode touches arrive only as such commands,
// from the client touch buttons created in UpdateTouch.
void CHudMOTD :: HandleButtonsDown( int ButtonPressed )
{
	if( !m_bShow )
		return;

	m_iButtons |= ButtonPressed & ( IN_FORWARD | IN_BACK | IN_ATTACK );

#ifdef _CSWEB_ENABLE
	if( ButtonPressed & IN_ATTACK )
		SendPageClick( true );
#endif
}

void CHudMOTD :: HandleButtonsUp( int ButtonPressed )
{
	// only buttons that were pressed through HandleButtonsDown count,
	// so a desktop mouse release (which still reaches -attack) is ignored here
	const int released = m_iButtons & ButtonPressed & ( IN_FORWARD | IN_BACK | IN_ATTACK );
	m_iButtons &= ~released;

#ifdef _CSWEB_ENABLE
	if( released & IN_ATTACK )
		SendPageClick( false );
#endif
}

// ------------------------------------------------------------------ touch
// (Re)creates the client touch buttons for the given layout.
void CHudMOTD :: UpdateTouch( const Layout &l )
{
	m_touchLayout = l;

	if( !g_iMobileAPIVersion )
		return;

	gMobileAPI.pfnTouchRemoveButton( "_motd_*" );
	AddTouchButton( "_motd_page", "+attack",       l.bx,       l.by,   l.bw,       l.bh );
	AddTouchButton( "_motd_ok",   "cl_motd_close", l.btnX,     l.btnY, MOTD_BTN_W, MOTD_BTN_H );
	AddTouchButton( "_motd_up",   "+forward",      l.scrUpX,   l.btnY, MOTD_SCR_W, MOTD_BTN_H );
	AddTouchButton( "_motd_down", "+back",         l.scrDownX, l.btnY, MOTD_SCR_W, MOTD_BTN_H );
}

// ------------------------------------------------------------------ drawing
int CHudMOTD :: Draw( float fTime )
{
	gHUD.m_iNoConsolePrint &= ~( 1 << 1 );
	if( !m_bShow )
		return 1;

	if( cl_motd_hide->value )
	{
		Reset();
		return 1;
	}

#ifdef _CSWEB_ENABLE
	Layout l;
	CalcLayout( l );

	// resolution changed: resize the browser, the page stays loaded
	if( m_iBrowserW != l.bw || m_iBrowserH != l.bh )
	{
		CSW_Resize( l.bw, l.bh );
		m_iBrowserW = l.bw;
		m_iBrowserH = l.bh;
	}

	if( memcmp( &l, &m_touchLayout, sizeof( Layout ) ) != 0 )
		UpdateTouch( l );

	// hold-to-scroll (+forward / +back from the touch buttons)
	const int dir = ( ( m_iButtons & IN_FORWARD ) ? 1 : 0 ) - ( ( m_iButtons & IN_BACK ) ? 1 : 0 );
	if( dir )
		CSW_Scroll( dir * MOTD_SCROLL_SPEED * (float)gHUD.m_flTimeDelta ); // positive = scroll up

	// window frame
	DrawUtils::DrawRectangle( l.px, l.py, l.pw, l.ph );

	// OK button
	DrawUtils::DrawRectangle( l.btnX, l.btnY, MOTD_BTN_W, MOTD_BTN_H );

	int tw = gHUD.GetCharWidth( 'O' ) + gHUD.GetCharWidth( 'K' );
	int tx = l.btnX + ( MOTD_BTN_W - tw ) / 2;
	int ty = l.btnY + ( MOTD_BTN_H - MOTD_TEXT_H ) / 2;

	if( IsOverButton( l ) )
		DrawUtils::DrawHudString( tx, ty, ScreenWidth, "OK", 255, 255, 255 );
	else
		DrawUtils::DrawHudString( tx, ty, ScreenWidth, "OK", 255, 180, 0 );

	// scroll buttons (touch devices only, desktop has the mouse wheel)
	if( g_iMobileAPIVersion )
	{
		DrawUtils::DrawRectangle( l.scrUpX,   l.btnY, MOTD_SCR_W, MOTD_BTN_H );
		DrawUtils::DrawRectangle( l.scrDownX, l.btnY, MOTD_SCR_W, MOTD_BTN_H );

		const int sty = l.btnY + ( MOTD_BTN_H - MOTD_TEXT_H ) / 2;
		DrawUtils::DrawHudString( l.scrUpX   + ( MOTD_SCR_W - DrawUtils::HudStringLen( "Up" ) ) / 2, sty, ScreenWidth, "Up", 255, 180, 0 );
		DrawUtils::DrawHudString( l.scrDownX + ( MOTD_SCR_W - DrawUtils::HudStringLen( "Dn" ) ) / 2, sty, ScreenWidth, "Dn", 255, 180, 0 );
	}

	// the real cursor is forwarded to the page every frame
	int mx, my;
	gEngfuncs.GetMousePosition( &mx, &my );
	CSW_MouseMove( mx, my );

	// page
	CSW_Draw( l.bw, l.bh, l.bx, l.by );
#endif

	return 1;
}

// ------------------------------------------------------------------ server message
int CHudMOTD :: MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf )
{
	if( cl_motd_hide->value )
		return 1;

	if( m_iFlags & HUD_DRAW )
		Reset(); // clear the current MOTD in prep for this one

	BufferReader reader( pszName, pbuf, iSize );

	int is_finished = reader.ReadByte();
	m_szMOTD.Append( reader.ReadString() );
#ifdef _CSWEB_ENABLE
	if( is_finished )
	{
		CUtlString head( m_szMOTD );
		head.TrimWhitespace();

		CUtlString url;
		if( head.BStartsWithCaseless( "http://" ) || head.BStartsWithCaseless( "https://" ) )
			url = head;                                          // direct URL
		else if( head.String()[0] == '<' )
			url = MakeDataUrl( m_szMOTD );                       // raw HTML
		else
			url = MakeDataUrl( TextToHtml( m_szMOTD.String() ) ); // plain text

		CreateBrowser( url.String() );
	}
#endif
	return 1;
}