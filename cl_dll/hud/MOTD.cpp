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
#include "kbutton.h"
#include "triangleapi.h"
#include <string.h>
#include <stdio.h>
#include <string>
#include "draw_util.h"
#include "build.h"
#include "keydefs.h"
#ifdef _CSWEB_ENABLE
#include "csw_interface.h"
#endif

// ------------------------------------------------------------------ layout
#define MOTD_PAD       16
#define MOTD_BTN_W     120
#define MOTD_BTN_H     24
#define MOTD_BTN_GAP   12
#define MOTD_MAX_W     800
#define MOTD_MAX_H     600
#define MOTD_MARGIN    30
#define MOTD_TEXT_H    13   // высота строки шрифта HUD

// ------------------------------------------------------------------ mouse
static void MotdSetMouseVisible( bool visible )
{
	gEngfuncs.pfnSetMouseEnable( !visible );
}

static bool MotdGetMouse( int &x, int &y )
{
	gEngfuncs.GetMousePosition( &x, &y );
	return true;
}

// ------------------------------------------------------------------ helpers
static std::string MakeDataUrl( const std::string &html )
{
	static const char tbl[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	const unsigned char *s = (const unsigned char *)html.data();
	size_t len = html.size();

	std::string out = "data:text/html;charset=utf-8;base64,";
	out.reserve( out.size() + ( len + 2 ) / 3 * 4 );

	for( size_t i = 0; i < len; i += 3 )
	{
		unsigned v = s[i] << 16;
		if( i + 1 < len ) v |= s[i + 1] << 8;
		if( i + 2 < len ) v |= s[i + 2];

		out += tbl[( v >> 18 ) & 63];
		out += tbl[( v >> 12 ) & 63];
		out += ( i + 1 < len ) ? tbl[( v >> 6 ) & 63] : '=';
		out += ( i + 2 < len ) ? tbl[v & 63] : '=';
	}
	return out;
}

// Обычный текст оборачиваем в экранированный <pre>
static std::string TextToHtml( const char *text )
{
	std::string out =
		"<!DOCTYPE html><meta charset=\"utf-8\">"
		"<body style=\"margin:8px;background:#111;color:#ffb400;font:14px monospace\">"
		"<pre style=\"white-space:pre-wrap;margin:0\">";

	for( ; *text; text++ )
	{
		switch( *text )
		{
		case '<': out += "&lt;";  break;
		case '>': out += "&gt;";  break;
		case '&': out += "&amp;"; break;
		default:  out += *text;   break;
		}
	}
	out += "</pre></body>";
	return out;
}

static bool StartsWithNoCase( const char *s, const char *prefix )
{
	while( *prefix )
	{
		char a = *s++, b = *prefix++;
		if( a >= 'A' && a <= 'Z' ) a += 32;
		if( a != b ) return false;
	}
	return true;
}

// ------------------------------------------------------------------ commands
void Close_f( void )
{
	gHUD.m_MOTD.Reset();
}

void OpenURL_f( void )
{
	if( gEngfuncs.Cmd_Argc() < 2 )
		return;

#ifdef _CSWEB_ENABLE
	gHUD.m_MOTD.Reset();
	gHUD.m_MOTD.m_szUrl = gEngfuncs.Cmd_Argv( 1 );
	gHUD.m_MOTD.m_iFlags |= HUD_DRAW;
	gHUD.m_MOTD.m_bShow = true;
#endif
}

// ------------------------------------------------------------------ CHudMOTD
int CHudMOTD :: Init( void )
{
	m_bMouseFree = false;
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
	m_szUrl.clear();
	m_bShow = false;
	m_bBrowserCreated = false;
	m_bFocused = false;
#ifdef _CSWEB_ENABLE
	CSW_SetFocus( false );
	CSW_Destroy();
#endif
	if( m_bMouseFree )
	{
		MotdSetMouseVisible( false );
		m_bMouseFree = false;
	}
}

void CHudMOTD :: CalcLayout( Layout &l ) const
{
	const int maxW = MOTD_MAX_W + MOTD_PAD * 2;
	const int maxH = MOTD_MAX_H + MOTD_PAD * 2 + MOTD_BTN_GAP + MOTD_BTN_H;

	l.pw = ScreenWidth  - MOTD_MARGIN * 2;
	l.ph = ScreenHeight - MOTD_MARGIN * 2;
	if( l.pw > maxW ) l.pw = maxW;
	if( l.ph > maxH ) l.ph = maxH;

	l.px = ( ScreenWidth  - l.pw ) / 2;
	l.py = ( ScreenHeight - l.ph ) / 2;

	// браузер: внутри рамки с padding, над кнопкой
	l.bw = l.pw - MOTD_PAD * 2;
	l.bh = l.ph - MOTD_PAD * 2 - MOTD_BTN_GAP - MOTD_BTN_H;
	l.bx = l.px + MOTD_PAD;
	l.by = l.py + MOTD_PAD;

	// кнопка OK: по центру внизу
	l.btnX = l.px + ( l.pw - MOTD_BTN_W ) / 2;
	l.btnY = l.py + l.ph - MOTD_PAD - MOTD_BTN_H;
}

bool CHudMOTD :: IsOverButton( const Layout &l ) const
{
	int mx, my;
	if( !MotdGetMouse( mx, my ) )
		return false;

	return mx >= l.btnX && mx < l.btnX + MOTD_BTN_W &&
	       my >= l.btnY && my < l.btnY + MOTD_BTN_H;
}

// Вызывать из HUD_Key_Event. true = клавиша обработана окном.
bool CHudMOTD :: KeyEvent( int down, int keynum )
{
#ifdef _CSWEB_ENABLE
	// Без окна CSW_Key только отслеживает модификаторы (Shift/Ctrl/Alt/Caps),
	// чтобы их состояние было верным в момент открытия MOTD.
	if( !m_bShow )
	{
		// CSW_Key( down, keynum );
		return false;
	}
#else
	if( !m_bShow )
		return false;
#endif

	if( keynum == K_ESCAPE )
	{
		if( down )
			Reset();
		return true;
	}

	if( keynum == K_MOUSE1 )
	{
		Layout l;
		CalcLayout( l );
		if( IsOverButton( l ) )
		{
			if( !down )
				Reset();       // закрываем по отпусканию
			return true;
		}
		// иначе клик по странице: падаем дальше в CSW_Key
	}

#ifdef _CSWEB_ENABLE
	return CSW_Key( down, keynum );
#else
	return false;
#endif
}

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

	// браузер создаём лениво; при смене разрешения пересоздаём
	if( !m_bBrowserCreated || m_iBrowserW != l.bw || m_iBrowserH != l.bh )
	{
		CSW_SetFocus( false );
		CSW_Destroy();
		CSW_Create( l.bw, l.bh, m_szUrl.c_str() );
		m_iBrowserW = l.bw;
		m_iBrowserH = l.bh;
		m_bBrowserCreated = true;
		m_bFocused = false;
	}

	// рамка
	DrawUtils::DrawRectangle( l.px, l.py, l.pw, l.ph );

	// кнопка OK
	DrawUtils::DrawRectangle( l.btnX, l.btnY, MOTD_BTN_W, MOTD_BTN_H );

	int tw = gHUD.GetCharWidth( 'O' ) + gHUD.GetCharWidth( 'K' );
	int tx = l.btnX + ( MOTD_BTN_W - tw ) / 2;
	int ty = l.btnY + ( MOTD_BTN_H - MOTD_TEXT_H ) / 2;

	if( IsOverButton( l ) )
		DrawUtils::DrawHudString( tx, ty, ScreenWidth, "OK", 255, 255, 255 );
	else
		DrawUtils::DrawHudString( tx, ty, ScreenWidth, "OK", 255, 180, 0 );

	// курсор: опрашиваем каждый кадр и пересылаем в страницу
	int mx, my;
	if( MotdGetMouse( mx, my ) )
		CSW_MouseMove( mx, my );

	// страница
	CSW_Draw( l.bw, l.bh, l.bx, l.by );

	// фокус и свободная мышь - после первого CSW_Draw (уже известны rx1..ry2)
	if( !m_bFocused )
	{
		CSW_SetFocus( true );
		MotdSetMouseVisible( true );
		m_bMouseFree = true;
		m_bFocused = true;
	}
#endif

	return 1;
}

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
		const char *s = m_szMOTD.String();
		while( *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' )
			s++;

		if( StartsWithNoCase( s, "http://" ) || StartsWithNoCase( s, "https://" ) )
			m_szUrl = s;                                        // прямой URL
		else if( *s == '<' )
			m_szUrl = MakeDataUrl( m_szMOTD.String() );         // сырой HTML
		else
			m_szUrl = MakeDataUrl( TextToHtml( m_szMOTD.String() ) ); // обычный текст
		
		m_iFlags |= HUD_DRAW;
		m_bShow = true;
	}
#endif
	return 1;
}