/*
spectator_gui.cpp - HUD Overlays
Copyright (C) 2015 a1batross

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

In addition, as a special exception, the author gives permission to
link the code of this program with the Half-Life Game Engine ("HL
Engine") and Modified Game Libraries ("MODs") developed by Valve,
L.L.C ("Valve").  You must obey the GNU General Public License in all
respects for all of the code used other than the HL Engine and MODs
from Valve.  If you modify this file, you may extend this exception
to your version of the file, but you are not obligated to do so.  If
you do not wish to do so, delete this exception statement from your
version.
*/

#include <string.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include "vgui_parser.h"
#include "triangleapi.h"
#include "draw_util.h"
#include "spectator_layout.h"
#include "text_utf8.h"
#include "keydefs.h"

#include <string>

// the MOTD window of the embedded browser takes the mouse while it is shown
#include "chrome/chrome_host.h"

static bool ModalWindowShown( void )
{
	return Chrome::IsMotdShown();
}

/*
 * We will draw all elements inside a box. It's size 16x10.
 */

#define XPOS( x ) ( (x) / 16.0f )
#define YPOS( y ) ( (y) / 10.0f  )

#define INT_XPOS(x) int(XPOS(x) * ScreenWidth)
#define INT_YPOS(y) int(YPOS(y) * ScreenHeight)

// The black bars at the top and at the bottom of the screen, as in the Steam version:
// 64 of 480 lines (144 pixels at 1080), black with the alpha 196 (measured on its screenshots)
#define BAR_ALPHA 196

int CHudSpectatorGui::BarHeight( void )
{
	return SpecBar::BarHeight( ScreenHeight );
}

int CHudSpectatorGui::Init()
{
	if( !g_iXash )
		return 1;

	HOOK_MESSAGE( gHUD.m_SpectatorGui, SpecHealth );
	HOOK_MESSAGE( gHUD.m_SpectatorGui, SpecHealth2 );

	HOOK_COMMAND( gHUD.m_SpectatorGui, "_spec_toggle_menu", ToggleSpectatorMenu );
	HOOK_COMMAND( gHUD.m_SpectatorGui, "_spec_toggle_menu_options", ToggleSpectatorMenuOptions );
	// close
	// help
	// settings
	// pip
	// autodirector
	// showscores

	HOOK_COMMAND( gHUD.m_SpectatorGui, "_spec_toggle_menu_options_settings", ToggleSpectatorMenuOptionsSettings );
	// settings
	// // chat msgs
	// // show status
	// // view cone
	// // player names

	HOOK_COMMAND( gHUD.m_SpectatorGui, "_spec_toggle_menu_spectate_options", ToggleSpectatorMenuSpectateOptions );
	// chase map overview
	// free map overview
	// first person
	// free look
	// free chase camera
	// locked chase camera

	HOOK_COMMAND( gHUD.m_SpectatorGui, "_spec_toggle_menu_players", ToggleSpectatorMenuPlayers );
	HOOK_COMMAND( gHUD.m_SpectatorGui, "_spec_follow_item", SpecFollowItem );
	HOOK_COMMAND( gHUD.m_SpectatorGui, "_spec_players_scroll", SpecPlayersScroll );

	HOOK_COMMAND_FUNC( "_spec_find_next_player_reverse", gHUD.m_Spectator.FindNextPlayer, true );
	HOOK_COMMAND_FUNC( "_spec_find_next_player", gHUD.m_Spectator.FindNextPlayer, false );

	gHUD.AddHudElem(this);
	m_iFlags = HUD_DRAW;
	m_menuFlags = 0;
	m_hTimerTexture = 0;
	m_iPlayerCount = 0;
	m_iPlayerFirst = 0;
	m_iPlayerTouch = -1;
	m_iPlayerTouchScroll = 0;
	m_iSettingsW = 0;
	m_bTouchClientOnly = false;
	return 1;
}

int CHudSpectatorGui::VidInit()
{
	if( !g_iXash )
	{
		ConsolePrint("Warning: CHudSpectatorGui is disabled! Dude, are you running me on old GoldSrc?\n");
		m_iFlags = 0;
		return 0;
	}

	FreeStatusBarText();

	// the timer icon is scaled with the bar (as in the Steam version), so it is filtered
	m_hTimerTexture = gRenderAPI.GL_LoadTexture("gfx/vgui/timer.tga", NULL, 0, TF_NOMIPMAP|TF_CLAMP );
	m_hChecked     = gRenderAPI.GL_LoadTexture("gfx/vgui/640_checked.tga", NULL, 0, TF_NEAREST | TF_NOMIPMAP | TF_CLAMP );
	m_hArrowDown   = gRenderAPI.GL_LoadTexture("gfx/vgui/1920_arrowdown.tga", NULL, 0, TF_NEAREST | TF_NOMIPMAP | TF_CLAMP );
	m_hArrowUp     = gRenderAPI.GL_LoadTexture("gfx/vgui/1920_arrowup.tga", NULL, 0, TF_NEAREST | TF_NOMIPMAP | TF_CLAMP );
	m_hArrowLeft   = gRenderAPI.GL_LoadTexture("gfx/vgui/1920_arrowleft.tga", NULL, 0, TF_NEAREST | TF_NOMIPMAP | TF_CLAMP );
	m_hArrowRight  = gRenderAPI.GL_LoadTexture("gfx/vgui/1920_arrowright.tga", NULL, 0, TF_NEAREST | TF_NOMIPMAP | TF_CLAMP );
	return 1;
}

void CHudSpectatorGui::Shutdown()
{
	FreeStatusBarText();
	gRenderAPI.GL_FreeTexture( m_hTimerTexture );
	gRenderAPI.GL_FreeTexture( m_hChecked );
	gRenderAPI.GL_FreeTexture( m_hArrowDown );
	gRenderAPI.GL_FreeTexture( m_hArrowUp );
	gRenderAPI.GL_FreeTexture( m_hArrowLeft );
	gRenderAPI.GL_FreeTexture( m_hArrowRight );
}



//
// The menus at the bottom as in the Steam version (see SpecMenu in spectator_layout.h)
//
namespace {

enum
{
	ST_OPTIONS = 0, ST_PLAYER, ST_VIEW,
	ST_CLOSE, ST_HELP, ST_SETTINGS, ST_PIP, ST_AUTODIRECTOR, ST_SHOWSCORES,
	ST_CHAT, ST_STATUS, ST_CONE, ST_NAMES,
	ST_MODE1, ST_MODE2, ST_MODE3, ST_MODE4, ST_MODE5, ST_MODE6,
	ST_PLAYER_ITEM,
	ST_COUNT = ST_PLAYER_ITEM + SpecMenu::PLAYERS_VISIBLE
};

CTextTexture g_specMenuText[ST_COUNT];

const int MENU_R = 255, MENU_G = 176, MENU_B = 0; // "BaseText"

void FrameRect( const SpecMenu::Rect &r, int red, int green, int blue, int alpha )
{
	FillRGBABlend( r.x, r.y, r.w, 1, red, green, blue, alpha );
	FillRGBABlend( r.x, r.y + r.h - 1, r.w, 1, red, green, blue, alpha );
	FillRGBABlend( r.x, r.y + 1, 1, r.h - 2, red, green, blue, alpha );
	FillRGBABlend( r.x + r.w - 1, r.y + 1, 1, r.h - 2, red, green, blue, alpha );
}

// a box of the Steam version: dark, a frame of 1 pixel ("BorderBright")
void Box( const SpecMenu::Rect &r, int bgAlpha )
{
	FillRGBABlend( r.x + 1, r.y + 1, r.w - 2, r.h - 2, 0, 0, 0, bgAlpha );
	FrameRect( r, 188, 112, 0, 128 );
}

// triangles of 1 pixel rows: dir 0 - left, 1 - right, 2 - down, 3 - up
void Triangle( int x, int y, int w, int h, int dir, int red, int green, int blue )
{
	if( dir == 2 || dir == 3 )
	{
		for( int i = 0; i < h; ++i )
		{
			const int row = dir == 2 ? i : h - 1 - i;
			FillRGBABlend( x + row, y + i, w - row * 2, 1, red, green, blue, 255 );
		}
		return;
	}

	const int half = h / 2;
	for( int i = 0; i < h; ++i )
	{
		const int d = i <= half ? i : h - 1 - i; // 0 at the top and the bottom, half in the middle
		const int len = w * ( d + 1 ) / ( half + 1 );
		if( len <= 0 )
			continue;
		if( dir == 1 )
			FillRGBABlend( x, y + i, len, 1, red, green, blue, 255 );
		else
			FillRGBABlend( x + w - len, y + i, len, 1, red, green, blue, 255 );
	}
}

// the check mark of a menu item (10 x 11 at 1080)
void CheckMark( int x, int y, int size, int red, int green, int blue )
{
	const int t = size / 5 > 1 ? size / 5 : 1;          // the thickness
	const int knee = size * 2 / 5;                       // the short stroke goes down to the right
	for( int i = 0; i <= knee; ++i )
		FillRGBABlend( x + i, y + size / 2 + i, t, t, red, green, blue, 255 );
	for( int i = 0; i <= size - knee; ++i )               // the long stroke goes up to the right
		FillRGBABlend( x + knee + i, y + size / 2 + knee - i * ( size / 2 + knee ) / ( size - knee ), t, t, red, green, blue, 255 );
}

struct SpecPainter
{
	float scale;
	int em, emHud;
	bool antialias;

	// the width of a text in the units of the HUD (0 without the Windows fonts)
	int Width( int slot, const char *text, int maxW ) const
	{
		CTextTexture &tex = g_specMenuText[slot];
		const std::string utf8 = TextUtf::ToUtf8( text ? text : "" );
		if( !tex.Update( utf8, "Verdana", em, 400, antialias, (int)( maxW * scale )) || !tex.Valid() )
			return 0;
		return (int)( tex.Width() / scale + 0.999f );
	}

	// a text of the menus: "DefaultSmall" (Verdana, thin); x and capTop in the units of the HUD
	void Text( int slot, const char *text, int x, int capTop, int maxW, int red, int green, int blue ) const
	{
		CTextTexture &tex = g_specMenuText[slot];
		const std::string utf8 = TextUtf::ToUtf8( text ? text : "" );

		if( tex.Update( utf8, "Verdana", em, 400, antialias, (int)( maxW * scale )))
		{
			if( tex.Valid() )
			{
				const int top = SpecBar::CellTopFromCapTop( (float)capTop, emHud );
				tex.Draw( x * scale, top * scale, red, green, blue );
			}
		}
		else if( text && text[0] )
		{
			DrawUtils::DrawHudString( x, capTop - gHUD.GetCharHeight() / 4, x + maxW, text, red, green, blue );
		}
	}
};

const char *SpecModeName( int mode )
{
	switch( mode )
	{
	case 1: return "Locked Chase Camera";
	case 2: return "Free Chase Camera";
	case 3: return "Free look";
	case 4: return "First Person";
	case 5: return "Free Map Overview";
	case 6: return "Chase Map Overview";
	}
	return "Spectate Options";
}

// is the mouse cursor (seen while the menus are open) over the rectangle
bool CursorIn( const SpecMenu::Rect &r )
{
	int mx = 0, my = 0;
	gEngfuncs.GetMousePosition( &mx, &my );
	if( TrueWidth <= 0 || TrueHeight <= 0 )
		return false;
	const float x = mx * (float)ScreenWidth / TrueWidth;
	const float y = my * (float)ScreenHeight / TrueHeight;
	return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// a touch button over a rectangle of the layout (the touch buttons are placed in parts of the screen)
void AddTouch( const char *name, const char *command, const SpecMenu::Rect &r )
{
	static byte color[4] = { 0, 0, 0, 0 };
	const float w = (float)ScreenWidth, h = (float)ScreenHeight;
	gMobileAPI.pfnTouchAddClientButton( name, "*white", command,
		r.x / w, r.y / h, ( r.x + r.w ) / w, ( r.y + r.h ) / h, color, 0, 1.0f, 0 );
}

} // namespace


// The upper bar as in the Steam version (see spectator_layout.h): the scores of the teams on the left of the
// divider, the money (or the map name for HLTV) and the time on the right of it, in the font of
// resource/ClientScheme.res drawn by Windows. Without the Windows fonts the font of the HUD is used.
void CHudSpectatorGui::DrawStatusBar( void )
{
	const int textR = 255, textG = 176, textB = 0; // "BaseText" of ClientScheme.res

	const float scale = gHUD.m_flScale > 0.0f ? gHUD.m_flScale : 1.0f; // pixels of the screen per unit of the HUD
	const SpecBar::Layout l = SpecBar::Compute( ScreenWidth, ScreenHeight );

	// the font: its size depends on the real height of the screen
	const SpecBar::Font font = SpecBar::FontFor( TrueHeight );
	const int em = SpecBar::FontEm( font.tall );
	const int emHud = (int)( em / scale + 0.5f );
	const int row1 = SpecBar::CellTopFromCapTop( SpecBar::Row1CapTop( ScreenHeight ), emHud );
	const int row2 = SpecBar::CellTopFromCapTop( SpecBar::Row2CapTop( ScreenHeight ), emHud );

	// the divider ("BorderBright" of the scheme)
	FillRGBABlend( l.dividerX, l.dividerY, l.dividerW, l.dividerH, 188, 112, 0, 128 );

	char ct[32], t[32], info[96];
	snprintf( ct, sizeof( ct ), "%d", label.m_iCounterTerrorists );
	snprintf( t, sizeof( t ), "%d", label.m_iTerrorists );

	if( gEngfuncs.IsSpectateOnly() )
		snprintf( info, sizeof( info ), "%s", label.m_szMap );
	else
		snprintf( info, sizeof( info ), "$%d", gHUD.m_Money.GetMoney() );

	const bool showTimer = !m_bBombPlanted;

	struct Item
	{
		CTextTexture *tex;
		const char *text;
		int x, y;
		bool rightAligned;
	};

	const Item items[] =
	{
		{ &m_txtCTLabel, "Counter-Terrorists :", l.teamLabelRight, row1, true },
		{ &m_txtTLabel,  "Terrorists :",         l.teamLabelRight, row2, true },
		{ &m_txtCTValue, ct,                     l.teamValueX,     row1, false },
		{ &m_txtTValue,  t,                      l.teamValueX,     row2, false },
		{ &m_txtInfo,    info,                   l.infoX,          row1, false },
		{ &m_txtTimer,   showTimer ? label.m_szTimer : "", l.timerTextX, row2, false },
	};

	for( const Item &it : items )
	{
		if( it.tex->Update( it.text, "Verdana", em, 600, font.antialias ))
		{
			if( !it.tex->Valid() )
				continue;

			const float w = it.tex->Width() / scale;
			const float x = it.rightAligned ? it.x - w : (float)it.x;
			it.tex->Draw( x * scale, it.y * scale, textR, textG, textB );
		}
		else if( it.text[0] )
		{
			// no Windows fonts
			if( it.rightAligned )
				DrawUtils::DrawHudStringReverse( it.x, it.y, 0, it.text, textR, textG, textB );
			else
				DrawUtils::DrawHudString( it.x, it.y, ScreenWidth, it.text, textR, textG, textB );
		}
	}

	// the timer icon
	if( showTimer && m_hTimerTexture )
	{
		gRenderAPI.GL_SelectTexture( 0 );
		gRenderAPI.GL_Bind( 0, m_hTimerTexture );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		DrawUtils::Draw2DQuad( l.timerIconX * scale, l.timerIconY * scale,
							   ( l.timerIconX + l.timerIconSize ) * scale, ( l.timerIconY + l.timerIconSize ) * scale );
		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	}
}

void CHudSpectatorGui::FreeStatusBarText( void )
{
	for( CTextTexture &t : g_specMenuText )
		t.Free();

	m_txtCTLabel.Free();
	m_txtTLabel.Free();
	m_txtCTValue.Free();
	m_txtTValue.Free();
	m_txtInfo.Free();
	m_txtTimer.Free();
}

int CHudSpectatorGui::Draw( float flTime )
{
	if( !g_iUser1 )
	{
		if( m_menuFlags & ROOT_MENU )
		{
			UserCmd_ToggleSpectatorMenu(); // this will remove any submenus;
			m_menuFlags = 0;
		}
		return 1;
	}

	// function name says it
	CalcAllNeededData( );

	// the MOTD window takes the mouse while it is shown, the menus get it back when it is closed
	SyncTouchMode();

	if( m_menuFlags & MENU_PLAYERS )
		UpdatePlayerMenu();

	int r = 255, g = 140, b = 0;

	// at first, draw these silly black bars
	int startpos = 0;
	if( gHUD.m_Spectator.m_pip->value != INSET_OFF ) // pip adjust
	{
		startpos = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowWidth) + XRES(gHUD.m_Spectator.m_OverviewData.insetWindowX);
		startpos *= ScreenWidth / TrueWidth; // hud_scale adjust
	}
	const int barH = BarHeight();
	FillRGBABlend(startpos, 0, ScreenWidth - startpos, barH, 0, 0, 0, BAR_ALPHA);
	FillRGBABlend(0, ScreenHeight - barH, ScreenWidth, barH, 0, 0, 0, BAR_ALPHA);

	if ( gHUD.m_Spectator.m_drawstatus && gHUD.m_Spectator.m_drawstatus->value )
		DrawStatusBar();

	if( m_menuFlags & ROOT_MENU )
	{
		DrawSteamMenus();
	}
	else
	{
		// the name of the player in the middle of the lower bar
		int iLen = DrawUtils::HudStringLen( label.m_szNameAndHealth );
		GetTeamColor( r, g, b, g_PlayerExtraInfo[ g_iUser2 ].teamnumber );
		DrawUtils::DrawHudString( ScreenWidth * 0.5 - iLen * 0.5, ScreenHeight - BarHeight() / 2 - gHUD.GetCharHeight() * 0.5 , ScreenWidth,
								  label.m_szNameAndHealth, r, g, b );
	}

	return 1;
}

// The menus as in the Steam version: three lists and two buttons at the bottom, the menus open above them
void CHudSpectatorGui::DrawSteamMenus( void )
{
	const SpecMenu::Layout l = SpecMenu::Compute( ScreenWidth, ScreenHeight );
	const SpecBar::Font font = SpecBar::FontFor( TrueHeight );

	SpecPainter paint;
	paint.scale = gHUD.m_flScale > 0.0f ? gHUD.m_flScale : 1.0f;
	paint.em = SpecBar::FontEm( font.tall );
	paint.emHud = (int)( paint.em / paint.scale + 0.5f );
	paint.antialias = font.antialias;
	const int capH = (int)( paint.emHud * 0.727f + 0.5f );

	const int arrowW = l.U( 3.1f ) | 1;   // 7 at 1080
	const int arrowH = ( arrowW + 1 ) / 2; // 4

	// a list: its text and a small grey arrow on the right
	auto list = [&]( const SpecMenu::Rect &r, int slot, const char *text, int red, int green, int blue )
	{
		Box( r, 230 );
		const int arrowX = r.x + r.w - 1 - l.U( 4.0f ) - arrowW / 2;
		paint.Text( slot, text, r.x + l.comboTextInset, r.y + l.comboCapTop, arrowX - r.x - l.comboTextInset - 2, red, green, blue );
		Triangle( arrowX, r.y + ( r.h - arrowH + 1 ) / 2 + 1, arrowW, arrowH, 2, 176, 175, 173 );
	};

	// a button with an orange triangle (7 x 13 at 1080)
	auto button = [&]( const SpecMenu::Rect &r, int dir, int inset )
	{
		Box( r, 230 );
		const int h = l.U( 5.8f ) | 1;
		Triangle( r.x + inset, r.y + ( r.h - h ) / 2, l.U( 3.1f ), h, dir, MENU_R, MENU_G, MENU_B );
	};

	// a menu: the items, a check mark or a submenu arrow; lit - the item whose submenu is open
	// the item under the mouse cursor is lit (the cursor is seen while the menus take the mouse)
	const bool hover = m_bTouchClientOnly;
	const int litA = 122;

	// a menu: the items, a check mark or a submenu arrow; lit - the item whose submenu is open; textW - the
	// width the texts are cut at (0: the width of the menu)
	struct Item { int slot; const char *text; int check; bool submenu; };
	auto menu = [&]( const SpecMenu::Rect &m, const Item *items, int count, bool checks, int lit, int textW )
	{
		FillRGBABlend( m.x + 1, m.y + 1, m.w - 2, m.h - 2, 0, 0, 0, 245 );
		FrameRect( m, 188, 112, 0, 128 );

		for( int i = 0; i < count; ++i )
		{
			const SpecMenu::Rect it = SpecMenu::MenuItem( l, m, i );

			if( i == lit || ( hover && CursorIn( it )))
				FillRGBABlend( it.x, it.y, it.w, it.h, MENU_R, MENU_G, MENU_B, litA );

			const int capTop = it.y + ( it.h - capH + 1 ) / 2;
			const int x = m.x + ( checks ? l.menuCheckTextInset : l.menuTextInset );
			paint.Text( items[i].slot, items[i].text, x, capTop, textW > 0 ? textW : m.x + m.w - x - l.U( 8.0f ),
			            MENU_R, MENU_G, MENU_B );

			if( items[i].check > 0 )
			{
				const int size = l.U( 4.6f ); // 10 at 1080
				CheckMark( m.x + l.menuCheckX, it.y + ( it.h - size ) / 2, size, MENU_R, MENU_G, MENU_B );
			}

			if( items[i].submenu )
			{
				const int h = l.U( 6.7f ) | 1; // 15 at 1080
				Triangle( m.x + m.w - l.U( 6.7f ), it.y + ( it.h - h ) / 2, l.U( 3.6f ), h, 1, MENU_R, MENU_G, MENU_B );
			}
		}
	};

	// the controls at the bottom
	list( l.options, ST_OPTIONS, "Options", MENU_R, MENU_G, MENU_B );
	button( l.prev, 0, l.U( 5.3f ));

	{
		// the player and his health: "player (100)", in the colour of his team
		int r, g, b;
		GetTeamColor( r, g, b, g_PlayerExtraInfo[g_iUser2].teamnumber );
		list( l.player, ST_PLAYER, label.m_szNameAndHealth, r, g, b );
	}

	button( l.next, 1, l.U( 6.7f ));
	list( l.view, ST_VIEW, SpecModeName( g_iUser1 ), MENU_R, MENU_G, MENU_B );

	if( m_menuFlags & MENU_OPTIONS )
	{
		const Item items[] =
		{
			{ ST_CLOSE, "Close", 0, false },
			{ ST_HELP, "Help", 0, false },
			{ ST_SETTINGS, "Settings", 0, true },
			{ ST_PIP, "Picture-in-Picture", gHUD.m_Spectator.m_pip && gHUD.m_Spectator.m_pip->value != INSET_OFF, false },
			{ ST_AUTODIRECTOR, "Autodirector", gHUD.m_Spectator.m_autoDirector && gHUD.m_Spectator.m_autoDirector->value, false },
			{ ST_SHOWSCORES, "Show scores", gHUD.m_Scoreboard.m_bForceDraw || gHUD.m_Scoreboard.m_bShowscoresHeld, false },
		};
		const SpecMenu::Rect m = SpecMenu::MenuAbove( l, l.options, 6 );
		menu( m, items, 6, true, ( m_menuFlags & MENU_OPTIONS_SETTINGS ) ? 2 : -1, 0 );

		// the submenu of the settings is as wide as its texts (as in the Steam version)
		const Item settings[] =
		{
			{ ST_CHAT, "Chat messages", gHUD.m_Spectator.m_HUD_saytext && gHUD.m_Spectator.m_HUD_saytext->value, false },
			{ ST_STATUS, "Show status", gHUD.m_Spectator.m_drawstatus && gHUD.m_Spectator.m_drawstatus->value, false },
			{ ST_CONE, "View cone", gHUD.m_Spectator.m_drawcone && gHUD.m_Spectator.m_drawcone->value, false },
			{ ST_NAMES, "Player names", gHUD.m_Spectator.m_drawnames && gHUD.m_Spectator.m_drawnames->value, false },
		};
		const int settingsTextW = l.U( 200.0f );
		int widest = 0;
		for( const Item &it : settings )
		{
			const int w = paint.Width( it.slot, it.text, settingsTextW );
			if( w > widest )
				widest = w;
		}
		m_iSettingsW = widest > 0 ? SpecMenu::CheckMenuWidth( l, widest ) : 0;

		if( m_menuFlags & MENU_OPTIONS_SETTINGS )
			menu( SpecMenu::SubmenuRight( l, m, 2, 4, m_iSettingsW ), settings, 4, true, -1, settingsTextW );
	}

	if( m_menuFlags & MENU_SPEC_OPTIONS )
	{
		const Item modes[] =
		{
			{ ST_MODE6, SpecModeName( 6 ), 0, false },
			{ ST_MODE5, SpecModeName( 5 ), 0, false },
			{ ST_MODE4, SpecModeName( 4 ), 0, false },
			{ ST_MODE3, SpecModeName( 3 ), 0, false },
			{ ST_MODE2, SpecModeName( 2 ), 0, false },
			{ ST_MODE1, SpecModeName( 1 ), 0, false },
		};
		menu( SpecMenu::MenuAbove( l, l.view, 6 ), modes, 6, false, -1, 0 );
	}

	// the list of the players above the player: "name (TEAM)" in "BaseText", as in the Steam version
	if( m_menuFlags & MENU_PLAYERS )
	{
		const int visible = SpecMenu::PlayersVisible( l, m_iPlayerCount, ScreenHeight, BarHeight() );
		if( visible > 0 )
		{
			const bool scroll = m_iPlayerCount > visible;
			const SpecMenu::Rect m = SpecMenu::MenuAbove( l, l.player, visible );
			FillRGBABlend( m.x + 1, m.y + 1, m.w - 2, m.h - 2, 0, 0, 0, 245 );
			FrameRect( m, 188, 112, 0, 128 );

			const SpecMenu::Rect bar = SpecMenu::ScrollBar( l, m );
			const bool anyHover = hover && CursorIn( m );

			for( int i = 0; i < visible && m_iPlayerFirst + i < m_iPlayerCount; ++i )
			{
				const int idx = m_iPlayers[m_iPlayerFirst + i];
				SpecMenu::Rect it = SpecMenu::MenuItem( l, m, i );
				if( scroll )
					it.w = bar.x - it.x;

				// lit: the item under the cursor, or the player that is followed
				if( anyHover ? CursorIn( it ) : idx == g_iUser2 )
					FillRGBABlend( it.x, it.y, it.w, it.h, MENU_R, MENU_G, MENU_B, litA );

				char text[128];
				const char *name = g_PlayerInfoList[idx].name ? g_PlayerInfoList[idx].name : "";
				const char *team = g_PlayerExtraInfo[idx].teamname;
				if( team[0] )
					snprintf( text, sizeof( text ), "%s (%s)", name, team );
				else
					snprintf( text, sizeof( text ), "%s", name );

				const int x = m.x + l.menuTextInset;
				paint.Text( ST_PLAYER_ITEM + i, text, x, it.y + ( it.h - capH + 1 ) / 2, it.x + it.w - x - l.U( 2.0f ),
				            MENU_R, MENU_G, MENU_B );
			}

			if( scroll )
			{
				// the scroll bar of the Steam version: a line on its left, the buttons with small grey arrows
				// (in the middle of the buttons, the Steam version has them near the right side), the thumb in
				// a frame
				FillRGBABlend( bar.x, bar.y, 1, bar.h, 188, 112, 0, 128 );

				const SpecMenu::Rect up = SpecMenu::ScrollUp( l, m ), down = SpecMenu::ScrollDown( l, m );
				const SpecMenu::Rect thumb = SpecMenu::ScrollThumb( l, m, m_iPlayerFirst, visible, m_iPlayerCount );
				FrameRect( up, 188, 112, 0, 128 );
				FrameRect( down, 188, 112, 0, 128 );
				const int aw = l.U( 3.1f ) | 1, ah = ( aw + 1 ) / 2;
				Triangle( up.x + ( up.w - aw ) / 2, up.y + ( up.h - ah ) / 2, aw, ah, 3, 176, 175, 173 );
				Triangle( down.x + ( down.w - aw ) / 2, down.y + ( down.h - ah ) / 2, aw, ah, 2, 176, 175, 173 );
				FillRGBABlend( thumb.x + 1, thumb.y + 1, thumb.w - 2, thumb.h - 2, 24, 14, 0, 160 );
				FrameRect( thumb, 188, 112, 0, 128 );
			}
		}
	}
}

// The players that can be followed, in the order of their numbers: in a team, alive, not the local player
// (the Steam version lists the spectators and the dead too, though the server does not follow them)
int CHudSpectatorGui::BuildPlayerList( int *list )
{
	gHUD.m_Scoreboard.GetAllPlayersInfo();

	const cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	const int localIndex = local ? local->index : 0;
	int n = 0;

	for( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		if( i == localIndex || i == gHUD.m_Scoreboard.m_iPlayerNum || !g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0] )
			continue;

		const int team = g_PlayerExtraInfo[i].teamnumber;
		if(( team != TEAM_TERRORIST && team != TEAM_CT ) || g_PlayerExtraInfo[i].dead )
			continue;

		list[n++] = i;
	}

	return n;
}

// The list of the players is made again every frame (players come, go and die); the touch buttons of its items
// are made again only when their places change
void CHudSpectatorGui::UpdatePlayerMenu( void )
{
	m_iPlayerCount = BuildPlayerList( m_iPlayers );

	const SpecMenu::Layout l = SpecMenu::Compute( ScreenWidth, ScreenHeight );
	const int visible = SpecMenu::PlayersVisible( l, m_iPlayerCount, ScreenHeight, BarHeight() );
	const int maxFirst = m_iPlayerCount - visible > 0 ? m_iPlayerCount - visible : 0;
	if( m_iPlayerFirst > maxFirst )
		m_iPlayerFirst = maxFirst;
	if( m_iPlayerFirst < 0 )
		m_iPlayerFirst = 0;

	const int scroll = m_iPlayerCount > visible ? 1 : 0;
	if( !g_iMobileAPIVersion || ( visible == m_iPlayerTouch && scroll == m_iPlayerTouchScroll ))
		return;

	gMobileAPI.pfnTouchRemoveButton( "_spec_pl_*" );
	m_iPlayerTouch = visible;
	m_iPlayerTouchScroll = scroll;

	if( visible <= 0 )
		return;

	const SpecMenu::Rect m = SpecMenu::MenuAbove( l, l.player, visible );
	const SpecMenu::Rect bar = SpecMenu::ScrollBar( l, m );

	for( int i = 0; i < visible; ++i )
	{
		SpecMenu::Rect it = SpecMenu::MenuItem( l, m, i );
		if( scroll )
			it.w = bar.x - it.x;

		char name[32], cmd[64];
		snprintf( name, sizeof( name ), "_spec_pl_%d", i );
		snprintf( cmd, sizeof( cmd ), "_spec_follow_item %d", i );
		AddTouch( name, cmd, it );
	}

	if( scroll )
	{
		AddTouch( "_spec_pl_up", "_spec_players_scroll -1", SpecMenu::ScrollUp( l, m ));
		AddTouch( "_spec_pl_down", "_spec_players_scroll 1", SpecMenu::ScrollDown( l, m ));
	}
}

void CHudSpectatorGui::ClosePlayerMenu( void )
{
	m_menuFlags &= ~MENU_PLAYERS;
	m_iPlayerTouch = -1;
	if( g_iMobileAPIVersion )
		gMobileAPI.pfnTouchRemoveButton( "_spec_pl_*" );
}

// one menu is open at a time, as in the Steam version
void CHudSpectatorGui::CloseOtherMenus( int keep )
{
	if( keep != MENU_OPTIONS && ( m_menuFlags & MENU_OPTIONS ))
		UserCmd_ToggleSpectatorMenuOptions();
	if( keep != MENU_SPEC_OPTIONS && ( m_menuFlags & MENU_SPEC_OPTIONS ))
		UserCmd_ToggleSpectatorMenuSpectateOptions();
	if( keep != MENU_PLAYERS && ( m_menuFlags & MENU_PLAYERS ))
		ClosePlayerMenu();
}

// While the menus are open the mouse moves a cursor and clicks their touch buttons. The MOTD window of the server
// needs the mouse itself: the menus give it while the window is shown.
void CHudSpectatorGui::SyncTouchMode( void )
{
	if( !g_iMobileAPIVersion )
		return;

	const bool want = ( m_menuFlags & ROOT_MENU ) && !ModalWindowShown();

	// on: set every frame (a closed HUD menu turns the mode off), off: once
	if( want || m_bTouchClientOnly )
		gMobileAPI.pfnTouchSetClientOnly( want );

	m_bTouchClientOnly = want;
}

bool CHudSpectatorGui::KeyEvent( int down, int keynum )
{
	if( !( m_menuFlags & MENU_PLAYERS ) || ModalWindowShown() )
		return false;

	if( keynum != K_MWHEELUP && keynum != K_MWHEELDOWN )
		return false;

	if( down )
	{
		m_iPlayerFirst += keynum == K_MWHEELUP ? -1 : 1;
		UpdatePlayerMenu(); // keeps it in the list
	}
	return true;
}

void CHudSpectatorGui::UserCmd_ToggleSpectatorMenuPlayers()
{
	if( !(m_menuFlags & ROOT_MENU) || !g_iMobileAPIVersion )
		return;

	if( m_menuFlags & MENU_PLAYERS )
	{
		ClosePlayerMenu();
		return;
	}

	CloseOtherMenus( MENU_PLAYERS );
	m_menuFlags |= MENU_PLAYERS;
	m_iPlayerTouch = -1;
	m_iPlayerCount = BuildPlayerList( m_iPlayers );

	// the player that is followed is seen in the list
	const SpecMenu::Layout l = SpecMenu::Compute( ScreenWidth, ScreenHeight );
	const int visible = SpecMenu::PlayersVisible( l, m_iPlayerCount, ScreenHeight, BarHeight() );
	m_iPlayerFirst = 0;
	for( int i = 0; i < m_iPlayerCount; ++i )
	{
		if( m_iPlayers[i] == g_iUser2 && i >= visible )
			m_iPlayerFirst = i - visible / 2;
	}

	UpdatePlayerMenu();
}

void CHudSpectatorGui::UserCmd_SpecFollowItem()
{
	if( !( m_menuFlags & MENU_PLAYERS ) || gEngfuncs.Cmd_Argc() < 2 )
		return;

	const int pos = m_iPlayerFirst + atoi( gEngfuncs.Cmd_Argv( 1 ));
	const int index = pos >= 0 && pos < m_iPlayerCount ? m_iPlayers[pos] : 0;

	ClosePlayerMenu();

	if( index > 0 )
		gHUD.m_Spectator.FollowPlayer( index );
}

void CHudSpectatorGui::UserCmd_SpecPlayersScroll()
{
	if( !( m_menuFlags & MENU_PLAYERS ) || gEngfuncs.Cmd_Argc() < 2 )
		return;

	m_iPlayerFirst += atoi( gEngfuncs.Cmd_Argv( 1 ));
	UpdatePlayerMenu();
}

void CHudSpectatorGui::CalcAllNeededData( )
{
	// mapname
	if( !label.m_szMap[0] )
	{
		static char szMapNameStripped[55];
		const char *szMapName = gEngfuncs.pfnGetLevelName(); //  "maps/%s.bsp"
		strncpy( szMapNameStripped, szMapName + 5, sizeof( szMapNameStripped ) );
		szMapNameStripped[strlen(szMapNameStripped) - 4] = '\0';
		snprintf( label.m_szMap, sizeof( label.m_szMap ), "Map: %s", szMapNameStripped );
	}

	// team
	/*label.m_iTerrorists        = 0;
	label.m_iCounterTerrorists = 0;
	for( int i = 0; i < MAX_PLAYERS; i++ )
	{
		if( g_PlayerExtraInfo[i].dead )
			continue; // show remaining

		switch( g_PlayerExtraInfo[i].teamnumber )
		{
		case TEAM_CT:
			label.m_iCounterTerrorists++;
		case TEAM_TERRORIST:
			label.m_iTerrorists++;
		}
	}*/

	label.m_iCounterTerrorists = 0;
	label.m_iTerrorists = 0;
	for( int i = 1; i <= gHUD.m_Scoreboard.m_iNumTeams; i++ )
	{
		switch( g_TeamInfo[i].teamnumber )
		{
		case TEAM_CT:
			label.m_iCounterTerrorists = g_TeamInfo[i].frags;
			break;
		case TEAM_TERRORIST:
			label.m_iTerrorists = g_TeamInfo[i].frags;
			break;
		}
	}

	// timer
	// time must be positive
	if( !m_bBombPlanted )
	{
		int iMinutes = max( 0, (int)( gHUD.m_Timer.m_iTime + gHUD.m_Timer.m_fStartTime - gHUD.m_flTime ) / 60);
		int iSeconds = max( 0, (int)( gHUD.m_Timer.m_iTime + gHUD.m_Timer.m_fStartTime - gHUD.m_flTime ) - (iMinutes * 60));

		sprintf( label.m_szTimer, "%i:%02i", iMinutes, iSeconds );
	}

	// player name
	if( g_iUser2 > 0 && g_iUser2 < MAX_PLAYERS )
	{
		hud_player_info_t sInfo;
		GetPlayerInfo( g_iUser2, &sInfo );

		int iHealth = g_PlayerExtraInfo[g_iUser2].sb_health > 255 ? g_PlayerExtraInfo[g_iUser2].sb_health : g_PlayerExtraInfo[g_iUser2].health;

		snprintf( label.m_szNameAndHealth, sizeof( label.m_szNameAndHealth ),
				  "%s (%i)",  sInfo.name, iHealth );
	}
	else label.m_szNameAndHealth[0] = '\0';
}

void CHudSpectatorGui::InitHUDData()
{
	m_bBombPlanted = false;
	label.m_szMap[0] = '\0';
}

void CHudSpectatorGui::Reset()
{
	m_bBombPlanted = false;
	if( m_menuFlags & ROOT_MENU )
	{
		UserCmd_ToggleSpectatorMenu(); // this will remove any submenus;
		m_menuFlags = 0;
	}
}

int CHudSpectatorGui::MsgFunc_SpecHealth(const char *pszName, int iSize, void *buf)
{
	BufferReader reader( pszName, buf, iSize );

	int health = reader.ReadByte();

	g_PlayerExtraInfo[g_iUser2].health = health;
	gHUD.m_Health.m_iPlayerLastPointedAt = g_iUser2;

	return 1;
}

int CHudSpectatorGui::MsgFunc_SpecHealth2(const char *pszName, int iSize, void *buf)
{
	BufferReader reader( pszName, buf, iSize );

	int health = reader.ReadByte();
	int client = reader.ReadByte();

	g_PlayerExtraInfo[client].health = health;
	gHUD.m_Health.m_iPlayerLastPointedAt = g_iUser2;

	return 1;
}

// The touch buttons (they take the mouse clicks) lie over the controls drawn by DrawSteamMenus()
void CHudSpectatorGui::UserCmd_ToggleSpectatorMenu()
{
	if( !g_iMobileAPIVersion )
		return;

	if( !(m_menuFlags & ROOT_MENU) )
	{
		m_menuFlags |= ROOT_MENU;

		const SpecMenu::Layout l = SpecMenu::Compute( ScreenWidth, ScreenHeight );
		AddTouch( "_spec_menu_options", "_spec_toggle_menu_options", l.options );
		AddTouch( "_spec_menu_players", "_spec_toggle_menu_players", l.player );
		AddTouch( "_spec_menu_find_next_player_reverse", "_spec_find_next_player_reverse", l.prev );
		AddTouch( "_spec_menu_find_next_player", "_spec_find_next_player", l.next );
		AddTouch( "_spec_menu_spectate_options", "_spec_toggle_menu_spectate_options", l.view );
	}
	else
	{
		m_menuFlags &= ~ROOT_MENU;
		m_menuFlags &= ~MENU_OPTIONS;
		m_menuFlags &= ~MENU_OPTIONS_SETTINGS;
		m_menuFlags &= ~MENU_SPEC_OPTIONS;
		m_menuFlags &= ~MENU_PLAYERS;
		m_iPlayerTouch = -1;
		gMobileAPI.pfnTouchRemoveButton( "_spec_*" );
	}

	SyncTouchMode();
}

void CHudSpectatorGui::UserCmd_ToggleSpectatorMenuOptions()
{
	if( !(m_menuFlags & ROOT_MENU) || !g_iMobileAPIVersion )
		return;

	if( !(m_menuFlags & MENU_OPTIONS) )
	{
		CloseOtherMenus( MENU_OPTIONS );
		m_menuFlags |= MENU_OPTIONS;

		const SpecMenu::Layout l = SpecMenu::Compute( ScreenWidth, ScreenHeight );
		const SpecMenu::Rect m = SpecMenu::MenuAbove( l, l.options, 6 );
		AddTouch( "_spec_opt_close", "_spec_toggle_menu", SpecMenu::MenuItem( l, m, 0 ));
		AddTouch( "_spec_opt_help", "spec_help; _spec_toggle_menu_options", SpecMenu::MenuItem( l, m, 1 ));
		AddTouch( "_spec_opt_settings", "_spec_toggle_menu_options_settings", SpecMenu::MenuItem( l, m, 2 ));
		AddTouch( "_spec_opt_pip", "spec_pip t; _spec_toggle_menu_options", SpecMenu::MenuItem( l, m, 3 ));
		AddTouch( "_spec_opt_ad", "spec_autodirector t; _spec_toggle_menu_options", SpecMenu::MenuItem( l, m, 4 ));
		AddTouch( "_spec_opt_showscores", "_spec_toggle_menu_options; scoreboard", SpecMenu::MenuItem( l, m, 5 ));
	}
	else
	{
		m_menuFlags &= ~MENU_OPTIONS;
		m_menuFlags &= ~MENU_OPTIONS_SETTINGS;
		gMobileAPI.pfnTouchRemoveButton( "_spec_opt_*" );
	}
}

void CHudSpectatorGui::UserCmd_ToggleSpectatorMenuOptionsSettings()
{
	if( !(m_menuFlags & ROOT_MENU) || !g_iMobileAPIVersion )
		return;

	if( !(m_menuFlags & MENU_OPTIONS_SETTINGS) )
	{
		m_menuFlags |= MENU_OPTIONS_SETTINGS;

		const SpecMenu::Layout l = SpecMenu::Compute( ScreenWidth, ScreenHeight );
		const SpecMenu::Rect sub = SpecMenu::SubmenuRight( l, SpecMenu::MenuAbove( l, l.options, 6 ), 2, 4, m_iSettingsW );
		AddTouch( "_spec_opt_set_chat_msgs", "hud_saytext t; _spec_toggle_menu_options_settings", SpecMenu::MenuItem( l, sub, 0 ));
		AddTouch( "_spec_opt_set_status", "spec_drawstatus t; _spec_toggle_menu_options_settings", SpecMenu::MenuItem( l, sub, 1 ));
		AddTouch( "_spec_opt_set_draw_cones", "spec_drawcone t; _spec_toggle_menu_options_settings", SpecMenu::MenuItem( l, sub, 2 ));
		AddTouch( "_spec_opt_set_draw_names", "spec_drawnames t; _spec_toggle_menu_options_settings", SpecMenu::MenuItem( l, sub, 3 ));
	}
	else
	{
		m_menuFlags &= ~MENU_OPTIONS_SETTINGS;
		gMobileAPI.pfnTouchRemoveButton( "_spec_opt_set_*" );
	}
}

void CHudSpectatorGui::UserCmd_ToggleSpectatorMenuSpectateOptions()
{
	if( !(m_menuFlags & ROOT_MENU) || !g_iMobileAPIVersion )
		return;

	if( !(m_menuFlags & MENU_SPEC_OPTIONS) )
	{
		CloseOtherMenus( MENU_SPEC_OPTIONS );
		m_menuFlags |= MENU_SPEC_OPTIONS;

		const SpecMenu::Layout l = SpecMenu::Compute( ScreenWidth, ScreenHeight );
		const SpecMenu::Rect m = SpecMenu::MenuAbove( l, l.view, 6 );
		AddTouch( "_spec_spec_6", "spec_mode 6; _spec_toggle_menu_spectate_options", SpecMenu::MenuItem( l, m, 0 ));
		AddTouch( "_spec_spec_5", "spec_mode 5; _spec_toggle_menu_spectate_options", SpecMenu::MenuItem( l, m, 1 ));
		AddTouch( "_spec_spec_4", "spec_mode 4; _spec_toggle_menu_spectate_options", SpecMenu::MenuItem( l, m, 2 ));
		AddTouch( "_spec_spec_3", "spec_mode 3; _spec_toggle_menu_spectate_options", SpecMenu::MenuItem( l, m, 3 ));
		AddTouch( "_spec_spec_2", "spec_mode 2; _spec_toggle_menu_spectate_options", SpecMenu::MenuItem( l, m, 4 ));
		AddTouch( "_spec_spec_1", "spec_mode 1; _spec_toggle_menu_spectate_options", SpecMenu::MenuItem( l, m, 5 ));
	}
	else
	{
		m_menuFlags &= ~MENU_SPEC_OPTIONS;
		gMobileAPI.pfnTouchRemoveButton( "_spec_spec_*" );
	}
}
