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
#include "draw_util.h"
#include "build.h"
#include "motd_util.h"
#include <chrono>

#if XASH_WIN32 == 1 || XASH_PSVITA == 1
#define strcasestr strstr
#endif

int CHudMOTD :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( gHUD.m_MOTD, MOTD );

	cl_hide_motd = CVAR_CREATE("cl_hide_motd", "0", FCVAR_ARCHIVE); // hide motd
	// 0 - show MOTD inside the game as text, 1 - open URL and HTML MOTD in the system browser
	// (a value above 1 works like 1, so the values 2 and 3 that older builds saved in a config keep working)
	cl_motd_browser = CVAR_CREATE("cl_motd_browser", "1", FCVAR_ARCHIVE);
	// 1 - if HTML MOTD contains an iframe with an http(s) address, open that address instead of the whole page
	cl_motd_browser_iframe = CVAR_CREATE("cl_motd_browser_iframe", "1", FCVAR_ARCHIVE);
	ResetBrowserState();
	Reset();

	return 1;
}

int CHudMOTD :: VidInit( void )
{
	// Load sprites here
	ResetBrowserState();
	return 1;
}

// the MOTD can be opened in the browser of the system (hud/MOTD_browser.cpp): Windows, Linux, the BSDs, Android
#if XASH_WIN32 || XASH_LINUX || XASH_ANDROID || XASH_FREEBSD || XASH_NETBSD || XASH_OPENBSD
#define MOTD_SYSTEM_BROWSER 1
#endif

#if MOTD_SYSTEM_BROWSER
bool MOTD_OpenInBrowser( const char *target );
bool MOTD_SaveTempHtml( const char *html, size_t len, char *outPath, size_t outSize );
bool MOTD_BrowserOpensFiles( void ); // false: only addresses (Android), an HTML MOTD stays in the game

// The system browser is opened not more often than this: a server that sends its MOTD again and again
// must not flood the desktop with browser windows and throw the player out of the game every time.
static const double MOTD_BROWSER_COOLDOWN = 5.0;

static double MotdNowSeconds( void )
{
	return std::chrono::duration<double>( std::chrono::steady_clock::now().time_since_epoch() ).count();
}

static bool MotdBrowserAllowed( void )
{
	static double lastOpen = -1.0e9;
	const double now = MotdNowSeconds();

	if( now - lastOpen < MOTD_BROWSER_COOLDOWN )
	{
		gEngfuncs.Con_Printf( "MOTD: the browser was opened less than %d seconds ago, this MOTD is skipped\n", (int)MOTD_BROWSER_COOLDOWN );
		return false;
	}

	lastOpen = now;
	return true;
}

#endif // MOTD_SYSTEM_BROWSER

void CHudMOTD :: ResetBrowserState( void )
{
	m_bMotdStarted = false;
	m_iBrowserMode = BROWSER_NONE;
	m_iBrowserLen = 0;
	m_szBrowserMOTD[0] = 0;
}

// Returns true if this MOTD chunk was consumed and must not be shown in game.
// MOTD comes in chunks, so we decide what to do at the first chunk:
//  - text starting with http:// or https:// is opened in the browser as a link
//  - text starting with '<' is HTML, it is saved to a file and opened in the browser
//  - anything else is shown in game as usual
bool CHudMOTD :: HandleBrowserMOTD( const char *chunk, bool finished )
{
#if MOTD_SYSTEM_BROWSER
	int mode = (int)cl_motd_browser->value;

	if( mode < 1 )
	{
		m_bMotdStarted = false;
		return false;
	}

	if( !m_bMotdStarted )
	{
		const char *p = chunk;

		ResetBrowserState();
		m_bMotdStarted = true;

		while( *p && MotdUtil::IsBlank( *p ))
			p++;

		if( MotdUtil::IsHttpUrl( p ))
			m_iBrowserMode = BROWSER_URL;
		else if( MotdUtil::LooksLikeHtml( p ) && MOTD_BrowserOpensFiles( ))
			m_iBrowserMode = BROWSER_HTML;
	}

	if( m_iBrowserMode == BROWSER_NONE )
	{
		if( finished )
			m_bMotdStarted = false;

		return false; // usual text MOTD, show it in game
	}

	// accumulate
	int chunkLen = (int)strlen( chunk );
	int room = (int)sizeof( m_szBrowserMOTD ) - 1 - m_iBrowserLen;

	if( chunkLen > room )
		chunkLen = room;

	if( chunkLen > 0 )
	{
		memcpy( m_szBrowserMOTD + m_iBrowserLen, chunk, chunkLen );
		m_iBrowserLen += chunkLen;
		m_szBrowserMOTD[m_iBrowserLen] = 0;
	}

	if( !finished )
		return true;

	// whole MOTD received
	char safeUrl[4096];

	if( m_iBrowserMode == BROWSER_URL )
	{
		char *url = m_szBrowserMOTD;
		char *end;

		while( *url && MotdUtil::IsBlank( *url ))
			url++;

		// URL ends on the first whitespace
		for( end = url; *end && !MotdUtil::IsBlank( *end ); end++ );
		*end = 0;

		// only http(s) links, made safe for the command line of the browser
		if( !MotdUtil::MakeSafeUrl( url, safeUrl, sizeof( safeUrl )))
			gEngfuncs.Con_Printf( "MOTD: the address is not usable, skipped\n" );
		else if( MotdBrowserAllowed( ))
		{
			gEngfuncs.Con_Printf( "Opening MOTD in browser: %s\n", safeUrl );
			MOTD_OpenInBrowser( safeUrl );
		}
	}
	else if( m_iBrowserMode == BROWSER_HTML )
	{
		char iframeUrl[2048];
		char path[1024];

		if( !MotdBrowserAllowed( ))
		{
			// skipped, the message is printed
		}
		else if( cl_motd_browser_iframe->value != 0 && MotdUtil::ExtractIframeSrc( m_szBrowserMOTD, iframeUrl, sizeof( iframeUrl ))
			&& MotdUtil::MakeSafeUrl( iframeUrl, safeUrl, sizeof( safeUrl )))
		{
			// the page is only a wrapper around an iframe, open the iframe address directly
			gEngfuncs.Con_Printf( "Opening MOTD iframe in browser: %s\n", safeUrl );
			MOTD_OpenInBrowser( safeUrl );
		}
		else if( MOTD_SaveTempHtml( m_szBrowserMOTD, m_iBrowserLen, path, sizeof( path )))
		{
			gEngfuncs.Con_Printf( "Opening MOTD in browser: %s\n", path );
			MOTD_OpenInBrowser( path );
		}
		else gEngfuncs.Con_Printf( "Can't save MOTD to a temporary file\n" );
	}

	ResetBrowserState();
	return true;
#else
	(void)chunk; (void)finished;
	return false;
#endif
}

void CHudMOTD :: Reset( void )
{
	m_iFlags &= ~HUD_DRAW;  // start out inactive
	m_szMOTD.Clear();
	m_iLines = 0;
	m_bShow = false;
	ignoreThisMotd = false;
}

#define LINE_HEIGHT  13
#define ROW_GAP  13
#define ROW_RANGE_MIN 30
#define ROW_RANGE_MAX ( ScreenHeight - 100 )
int CHudMOTD :: Draw( float fTime )
{
	gHUD.m_iNoConsolePrint &= ~( 1 << 1 );
	if( !m_bShow )
		return 1;

	if( cl_hide_motd->value )
	{
		Reset();
		return 1;
	}

	gHUD.m_iNoConsolePrint |= 1 << 1;
	// find the top of where the MOTD should be drawn,  so the whole thing is centered in the screen
	int ypos = (ScreenHeight - LINE_HEIGHT * m_iLines)/2; // shift it up slightly
	char *ch = m_szMOTD.Access();
	int xpos = (ScreenWidth - gHUD.GetCharWidth( 'M' ) * m_iMaxLength) / 2;
	if( xpos < 30 ) xpos = 30;
	int xmax = xpos + gHUD.GetCharWidth( 'M' ) * m_iMaxLength;
	int height = LINE_HEIGHT * m_iLines;
	int ypos_r=ypos;
	if( height > ROW_RANGE_MAX )
	{
		ypos = ROW_RANGE_MIN + 7 + scroll;
		if( ypos  > ROW_RANGE_MIN + 4 )
			scroll-= (ypos - ( ROW_RANGE_MIN + 4))/3.0;
		if( ypos + height < ROW_RANGE_MAX )
			scroll+= (ROW_RANGE_MAX - (ypos + height))/ 3.0;
		ypos_r = ROW_RANGE_MIN;
		height = ROW_RANGE_MAX;
	}
	if( xmax > ScreenWidth - 30 ) xmax = ScreenWidth - 30;
	char *next_line;
	DrawUtils::DrawRectangle(xpos-5, ypos_r - 5, xmax - xpos+10, height + 10);
	while ( *ch )
	{
		int line_length = 0;  // count the length of the current line
		for ( next_line = ch; *next_line != '\n' && *next_line != 0; next_line++ )
			line_length += gHUD.GetCharWidth( (unsigned char)*next_line );
		char *top = next_line;
		if ( *top == '\n' )
			*top = 0;
		else
			top = NULL;

		// find where to start drawing the line
		if( (ypos > ROW_RANGE_MIN) && (ypos + LINE_HEIGHT <= ypos_r + height) )
			DrawUtils::DrawHudString( xpos, ypos, xmax, ch, 255, 180, 0 );

		ypos += LINE_HEIGHT;

		if ( top )  // restore 
			*top = '\n';
		ch = next_line;
		if ( *ch == '\n' )
			ch++;

		if ( ypos > (ScreenHeight - 20) )
			break;  // don't let it draw too low
	}
	
	return 1;
}

int CHudMOTD :: MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf )
{
	if( cl_hide_motd->value )
		return 1;

	if ( m_iFlags & HUD_DRAW )
	{
		Reset(); // clear the current MOTD in prep for this one
	}

	BufferReader reader( pszName, pbuf, iSize );

	int is_finished = reader.ReadByte();
	const char *motd_chunk = reader.ReadString();

	// URL or HTML MOTD goes to the system browser
	if( HandleBrowserMOTD( motd_chunk, is_finished != 0 ))
		return 1;

	if( ignoreThisMotd )
		return 1;

	m_szMOTD.Append( motd_chunk );

	// we still don't support html tags in motd :(
	if( strcasestr( m_szMOTD.String(), "<!DOCTYPE HTML>" ) )
	{
		Reset();
		ignoreThisMotd = true;
	}

	if ( is_finished )
	{
		int length = 0;
		
		m_iMaxLength = 0;
		m_iFlags |= HUD_DRAW;


		for ( const char *sz = m_szMOTD.String(); *sz != 0; sz++ )  // count the number of lines in the MOTD
		{
			if ( *sz == '\n' )
			{
				m_iLines++;
				if( length > m_iMaxLength )
				{
					m_iMaxLength = length;
					length = 0;
				}
			}
			length++;
		}
		
		m_iLines++;
		if( length > m_iMaxLength )
		{
			m_iMaxLength = length;
			length = 0;
		}
		m_bShow = true;
	}

	return 1;
}
