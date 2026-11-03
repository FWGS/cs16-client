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
// Scoreboard.cpp
//
// implementation of CHudScoreboard class
//
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "com_weapons.h"
#include "cdll_dll.h"
#include "draw_util.h"
#include "vgui_parser.h"
#include "eventscripts.h"
#include "scoreboard_layout.h"
#include "spectator_layout.h"
#include "text_utf8.h"

#include <algorithm>
#include <string>

namespace { void FreeSteamScoreboardText(); }

hud_player_info_t   g_PlayerInfoList[MAX_PLAYERS+1]; // player info from the engine
extra_player_info_t	g_PlayerExtraInfo[MAX_PLAYERS+1]; // additional player info sent directly to the client dll
team_info_t         g_TeamInfo[MAX_TEAMS+1];
hostage_info_t      g_HostageInfo[MAX_HOSTAGES+1];
int g_iUser1;
int g_iUser2;
int g_iUser3;
int g_iTeamNumber;


// X positions

int xstart, xend;
int ystart, yend;

static void GetSafeAreaInsets( float insets[4] )
{
	insets[0] = insets[1] = insets[2] = insets[3] = 0.0f;

	if( !g_iMobileAPIVersion || !gMobileAPI.pfnGetNativeObject )
		return;

	cvar_t *touch_safearea = gEngfuncs.pfnGetCvarPointer( "touch_safearea" );
	if( touch_safearea && !touch_safearea->value )
		return;

	// older engines don't know this object and return NULL
	const float *native = (const float *)gMobileAPI.pfnGetNativeObject( "WindowInsets" );
	if( !native )
		return;

	for( int i = 0; i < 4; i++ )
	{
		// sanity check, a safe area can't take more than half of the screen
		if( native[i] > 0.0f )
			insets[i] = native[i] < 0.5f ? native[i] : 0.5f;
	}

	if( insets[0] + insets[2] >= 1.0f || insets[1] + insets[3] >= 1.0f )
		insets[0] = insets[1] = insets[2] = insets[3] = 0.0f;
}

// Safe area in pixels: x, y, width, height
static void GetSafeAreaRect( float rect[4] )
{
	float insets[4];

	GetSafeAreaInsets( insets );

	rect[0] = ScreenWidth * insets[0];
	rect[1] = ScreenHeight * insets[1];
	rect[2] = ScreenWidth * ( 1.0f - insets[0] - insets[2] );
	rect[3] = ScreenHeight * ( 1.0f - insets[1] - insets[3] );
}

enum
{
	COL_NAME = 0,
	COL_ATTRIB,
	COL_HP,
	COL_MONEY,
	COL_KILLS,
	COL_DEATHS,
	COL_PING,
	TOTAL_COLUMNS
};

static struct Column
{
	int start, end;
	const char *name;

	Column() :
	    start( 0 ), end( 0 ), name( nullptr ) { }

	Column( int s, const char *n = nullptr, bool reverse = true )
	{
		name = n;
		end = 0;
		start = 0;

		if ( reverse )
		{
			start = s;
			if ( n )
				end = start - DrawUtils::HudStringLen( n );
		}
		else
		{
			start = s;
			if ( n )
				end = start + DrawUtils::HudStringLen( n );
		}
	}
} g_Columns[TOTAL_COLUMNS];

//#include "vgui_TeamFortressViewport.h"

int CHudScoreboard :: Init( void )
{
	gHUD.AddHudElem( this );

	// Hook messages & commands here
	HOOK_COMMAND( gHUD.m_Scoreboard, "+showscores", ShowScores );
	HOOK_COMMAND( gHUD.m_Scoreboard, "-showscores", HideScores );
	HOOK_COMMAND( gHUD.m_Scoreboard, "showscoreboard2", ShowScoreboard2 );
	HOOK_COMMAND( gHUD.m_Scoreboard, "hidescoreboard2", HideScoreboard2 );

	HOOK_MESSAGE( gHUD.m_Scoreboard, ScoreInfo );
	HOOK_MESSAGE( gHUD.m_Scoreboard, TeamScore );
	HOOK_MESSAGE( gHUD.m_Scoreboard, TeamInfo );

	InitHUDData();

	cl_showpacketloss = CVAR_CREATE( "cl_showpacketloss", "0", FCVAR_ARCHIVE );
	cl_showplayerversion = CVAR_CREATE( "cl_showplayerversion", "0", 0 );
	cl_show_scoreboard_on_death = CVAR_CREATE( "cl_show_scoreboard_on_death", "0", FCVAR_ARCHIVE );

	return 1;
}


int CHudScoreboard :: VidInit( void )
{
	FreeSteamScoreboardText();

	xstart = ScreenWidth * 0.125f;
	xend = ScreenWidth - xstart;
	ystart = 100;
	yend = ScreenHeight - ystart;
	m_bForceDraw = false;
	memset( m_flForceRect, 0, sizeof( m_flForceRect ));

	// Load sprites here
	return 1;
}

void CHudScoreboard :: InitHUDData( void )
{
	memset( g_PlayerExtraInfo, 0, sizeof g_PlayerExtraInfo );
	m_iLastKilledBy = 0;
	m_fLastKillTime = 0;
	m_iPlayerNum = 0;
	m_iNumTeams = 0;
	memset( g_TeamInfo, 0, sizeof g_TeamInfo );

	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		// a1ba: get the cl.playernum from the engine
		// it shouldn't ever change during normal gameplay
		if( !m_iPlayerNum && EV_IsLocal( i ))
			m_iPlayerNum = i;

		g_PlayerExtraInfo[i].sb_health = -1;
		g_PlayerExtraInfo[i].sb_account = -1;
	}

	m_iFlags &= ~HUD_DRAW;  // starts out inactive

	m_iFlags |= HUD_INTERMISSION; // is always drawn during an intermission
	m_iFlags |= HUD_THINK; // follows the players who leave and come (see Think)
	memset( m_bSlotUsed, 0, sizeof( m_bSlotUsed ));
}

// The team, the score and the rest of a slot belong to the player who had it. When he leaves, nothing of the server
// says so to the client, and a player who takes the slot later was shown in the old team, with the old score, till
// the server sent his own TeamInfo (on some servers only when he chose a team). Now a slot that empties is cleared,
// and a player who comes without a team is among the spectators (as the server makes him: "UNASSIGNED"), as in the
// Steam version.
void CHudScoreboard :: Think( void )
{
	bool changed = false;

	for( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		hud_player_info_t info = {};
		GetPlayerInfo( i, &info );
		const bool used = info.name && info.name[0];

		if( !used && m_bSlotUsed[i] )
		{
			memset( &g_PlayerExtraInfo[i], 0, sizeof( g_PlayerExtraInfo[i] ));
			g_PlayerExtraInfo[i].sb_health = -1;
			g_PlayerExtraInfo[i].sb_account = -1;
			changed = true;
		}
		else if( used && !m_bSlotUsed[i] && gHUD.m_Teamplay && !g_PlayerExtraInfo[i].teamname[0] )
		{
			strncpy( g_PlayerExtraInfo[i].teamname, "SPECTATOR", MAX_TEAM_NAME );
			g_PlayerExtraInfo[i].teamname[MAX_TEAM_NAME - 1] = 0;
			g_PlayerExtraInfo[i].teamnumber = TEAM_UNASSIGNED;
			changed = true;
		}

		m_bSlotUsed[i] = used;
	}

	if( changed )
		RebuildTeams();
}

bool CHudScoreboard :: ShouldDrawScoreboard() const
{
	if( m_bForceDraw )
		return true;

	if( m_bShowscoresHeld || gHUD.m_iIntermission )
		return true;

	if( cl_show_scoreboard_on_death && cl_show_scoreboard_on_death->value && gHUD.m_Health.m_iHealth <= 0 )
		return true;

	return false;
}

// Localize() gives the key itself if the strings of the game have no such record
// (then the table shows "#Cstrike_HEALTH"), so a plain default is used in that case
static const char *LocalizeOrDefault( const char *key, const char *fallback )
{
	const char *text = Localize( key );

	if( !text || text == key || text[0] == '#' )
		return fallback;

	return text;
}

//
// The table as in the Steam version (see scoreboard_layout.h), drawn by the fonts of Windows.
// Without them (or for "showscoreboard2") the old table below is drawn.
//
namespace {

enum
{
	PT_NAME = 0,
	PT_ATTRIB,
	PT_HP,
	PT_MONEY,
	PT_SCORE,
	PT_DEATHS,
	PT_PING,
	PT_COUNT
};

enum
{
	TT_NAME = 0,
	TT_SCORE,
	TT_PING,
	TT_COUNT
};

const int SB_MAX_SECTIONS = 8;

CTextTexture g_sbTitle;
CTextTexture g_sbHead[SbLayout::NUM_COLUMNS];
CTextTexture g_sbTeam[SB_MAX_SECTIONS][TT_COUNT];
CTextTexture g_sbPlayer[MAX_PLAYERS + 1][PT_COUNT];

void FreeSteamScoreboardText()
{
	g_sbTitle.Free();

	for( CTextTexture &t : g_sbHead )
		t.Free();

	for( auto &section : g_sbTeam )
		for( CTextTexture &t : section )
			t.Free();

	for( auto &player : g_sbPlayer )
		for( CTextTexture &t : player )
			t.Free();
}

// Draws a text of the scoreboard; x, capTop and maxWidth are in the units of the HUD.
// Returns false if the fonts of Windows are not available.
struct SbPainter
{
	float scale;
	int em, emHud;
	bool antialias;

	bool Text( CTextTexture &tex, const std::string &utf8, int x, int capTop, int r, int g, int b,
			   bool rightAligned, int maxWidth = 4096 ) const
	{
		if( !tex.Update( utf8, "Verdana", em, 600, antialias, (int)( maxWidth * scale )))
			return false;

		if( !tex.Valid() )
			return true; // nothing to draw

		const float w = tex.Width() / scale;
		const float left = rightAligned ? x - w : (float)x;
		const int top = SpecBar::CellTopFromCapTop( (float)capTop, emHud );

		tex.Draw( left * scale, top * scale, r, g, b );
		return true;
	}
};

std::string NumberText( int value )
{
	char buf[32];
	snprintf( buf, sizeof( buf ), "%d", value );
	return buf;
}

} // namespace

void CHudScoreboard :: Shutdown( void )
{
	FreeSteamScoreboardText();
}

bool CHudScoreboard :: DrawSteamStyle( void )
{
	const float scale = gHUD.m_flScale > 0.0f ? gHUD.m_flScale : 1.0f;
	SbLayout::Layout l = SbLayout::Compute( ScreenWidth, ScreenHeight );
	const SpecBar::Font font = SpecBar::FontFor( TrueHeight );

	SbPainter paint;
	paint.scale = scale;
	paint.em = SpecBar::FontEm( font.tall );
	paint.emHud = (int)( paint.em / scale + 0.5f );
	paint.antialias = font.antialias;

	const int headR = 255, headG = 176, headB = 0; // "BaseText"

	// the name of the server: if the fonts of Windows can't draw it, the old table is drawn
	const char *title = gHUD.m_szServerName[0] ? gHUD.m_szServerName : ( gHUD.m_Teamplay ? "TEAMS" : "PLAYERS" );
	const std::string titleUtf8 = TextUtf::ToUtf8( title );

	if( !g_sbTitle.Update( titleUtf8, "Verdana", paint.em, 600, paint.antialias, (int)( l.titleMaxW * scale )))
		return false;

	// the teams in the order of drawing: the most frags first, the spectators at the end
	int order[SB_MAX_SECTIONS];
	int sections = 0;
	int players = 0;

	if( gHUD.m_Teamplay )
	{
		// the scores of the teams (the same as the old table)
		for( int i = 1; i <= m_iNumTeams; i++ )
		{
			if( !g_TeamInfo[i].scores_overriden )
				g_TeamInfo[i].frags = g_TeamInfo[i].deaths = 0;
			g_TeamInfo[i].sumping = 0;
			g_TeamInfo[i].players = 0;
			g_TeamInfo[i].already_drawn = FALSE;
		}

		for( int i = 1; i < MAX_PLAYERS; i++ )
		{
			if( !g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0] || !g_PlayerExtraInfo[i].teamname[0] )
				continue;

			int j;
			for( j = 1; j <= m_iNumTeams; j++ )
				if( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ))
					break;

			if( j > m_iNumTeams )
				continue;

			if( !g_TeamInfo[j].scores_overriden )
			{
				g_TeamInfo[j].frags += g_PlayerExtraInfo[i].frags;
				g_TeamInfo[j].deaths += g_PlayerExtraInfo[i].deaths;
			}

			g_TeamInfo[j].sumping += g_PlayerInfoList[i].ping;
			g_TeamInfo[j].ownteam = g_PlayerInfoList[i].thisplayer ? TRUE : FALSE;
			g_TeamInfo[j].players++;
		}

		// as in the Steam version: the terrorists, the counter-terrorists, other teams (the most frags first),
		// the spectators at the end
		while( sections < SB_MAX_SECTIONS )
		{
			int best = 0, spectators = 0, terrorists = 0, cts = 0;
			int highest_frags = -99999, lowest_deaths = 99999;

			for( int i = 1; i <= m_iNumTeams; i++ )
			{
				if( g_TeamInfo[i].players <= 0 || g_TeamInfo[i].already_drawn )
					continue;

				if( !strnicmp( g_TeamInfo[i].name, "SPECTATOR", MAX_TEAM_NAME ))
				{
					spectators = i;
					continue;
				}

				if( g_TeamInfo[i].teamnumber == TEAM_TERRORIST && !terrorists )
					terrorists = i;
				else if( g_TeamInfo[i].teamnumber == TEAM_CT && !cts )
					cts = i;

				if( g_TeamInfo[i].frags > highest_frags
					|| ( g_TeamInfo[i].frags == highest_frags && g_TeamInfo[i].deaths < lowest_deaths ))
				{
					best = i;
					highest_frags = g_TeamInfo[i].frags;
					lowest_deaths = g_TeamInfo[i].deaths;
				}
			}

			if( terrorists )
				best = terrorists;
			else if( cts )
				best = cts;

			if( !best )
				best = spectators;

			if( !best )
				break;

			g_TeamInfo[best].already_drawn = TRUE;
			order[sections++] = best;
			players += g_TeamInfo[best].players;
		}
	}

	// the players who are not in a team (all the players if it is not team play)
	for( int i = 1; i < MAX_PLAYERS; i++ )
		if( g_PlayerInfoList[i].name && g_PlayerInfoList[i].name[0] && ( !gHUD.m_Teamplay || !g_PlayerExtraInfo[i].teamname[0] ))
			players++;

	// the window grows (and the rows get lower) to show all the players
	const int capHeight = (int)( paint.em * 0.727f / scale + 0.5f );
	SbLayout::Fit( l, ScreenHeight, sections, players, capHeight );

	// what the local player may see: a spectator has no columns of HP and Money; the bomb is shown to the
	// terrorists, the defuse kits to the counter-terrorists (and both to the spectators)
	const int localTeam = ( m_iPlayerNum > 0 && m_iPlayerNum <= MAX_PLAYERS ) ? g_PlayerExtraInfo[m_iPlayerNum].teamnumber : TEAM_UNASSIGNED;
	const bool localSpectator = localTeam != TEAM_TERRORIST && localTeam != TEAM_CT && gHUD.m_Teamplay;
	const bool moneyColumns = !localSpectator;
	const bool seeBomb = localTeam != TEAM_CT;
	const bool seeKits = localTeam != TEAM_TERRORIST;
	const int statusX = moneyColumns ? l.statusX : l.statusXNoMoney;

	// the window: "ListBG", the left column and the top row of it are half covered
	DrawUtils::FillRoundedRect( l.bgX, l.bgY, l.bgW, l.bgH, l.radius, true, true, 0, 0, 0, 128 );
	FillRGBABlend( l.panelX, l.bgY + l.radius, 1, l.bgH - l.radius * 2, 0, 0, 0, 64 );
	FillRGBABlend( l.bgX + l.radius, l.panelY, l.bgW - l.radius * 2, 1, 0, 0, 0, 64 );

	// the headers
	paint.Text( g_sbTitle, titleUtf8, l.titleX, l.titleCapTop, headR, headG, headB, false, l.titleMaxW );

	const char *const heads[SbLayout::NUM_COLUMNS] =
	{
		LocalizeOrDefault( "#Cstrike_HEALTH", "HP" ),
		LocalizeOrDefault( "#Cstrike_ACCOUNT", "Money" ),
		LocalizeOrDefault( "#PlayerScore", "Score" ),
		LocalizeOrDefault( "#PlayerDeath", "Deaths" ),
		LocalizeOrDefault( "#PlayerPing", "Latency" ),
	};

	for( int i = 0; i < SbLayout::NUM_COLUMNS; ++i )
	{
		const bool hidden = !moneyColumns && ( i == SbLayout::COL_HP || i == SbLayout::COL_MONEY );
		paint.Text( g_sbHead[i], hidden ? "" : TextUtf::ToUtf8( heads[i] ), l.colRight[i], l.headCapTop, headR, headG, headB, true );
	}

	// the line under the headers: "BorderDark"
	FillRGBABlend( l.lineX, l.headLineY, l.lineW, 1, 188, 112, 0, 128 );

	int sectionTop = l.headLineY;
	bool full = false;

	// draws the players of a team (or all the players who have no team if team is NULL)
	auto drawPlayers = [&]( const char *team, bool spectators, int y ) -> int
	{
		int list[MAX_PLAYERS + 1];
		int count = 0;

		for( int i = 1; i < MAX_PLAYERS; i++ )
		{
			if( !g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0] )
				continue;

			// without a team: in team play only the players who have no team, else all of them
			if( team ? stricmp( g_PlayerExtraInfo[i].teamname, team ) != 0 : ( gHUD.m_Teamplay && g_PlayerExtraInfo[i].teamname[0] != 0 ))
				continue;

			list[count++] = i;
		}

		// the most frags first, then the fewest deaths
		std::stable_sort( list, list + count, []( int a, int b )
		{
			if( g_PlayerExtraInfo[a].frags != g_PlayerExtraInfo[b].frags )
				return g_PlayerExtraInfo[a].frags > g_PlayerExtraInfo[b].frags;
			return g_PlayerExtraInfo[a].deaths < g_PlayerExtraInfo[b].deaths;
		});

		for( int n = 0; n < count; ++n )
		{
			if( y + l.rowH > l.bottom )
			{
				full = true;
				break;
			}

			const int idx = list[n];
			hud_player_info_t *pl_info = &g_PlayerInfoList[idx];
			extra_player_info_t *extra = &g_PlayerExtraInfo[idx];
			CTextTexture *tex = g_sbPlayer[idx];

			const float *colors = GetClientColor( idx );
			const int r = (int)( 255 * colors[0] ), g = (int)( 255 * colors[1] ), b = (int)( 255 * colors[2] );

			if( pl_info->thisplayer )
				FillRGBABlend( l.litX, y, l.litW, l.rowH, 255, 255, 255, 26 );

			const int capTop = y + l.rowCapTop;

			paint.Text( tex[PT_NAME], TextUtf::ToUtf8( pl_info->name ), l.nameX, capTop, r, g, b, false,
						statusX - l.nameX - (int)( 4 * l.u ));

			// the status, left aligned (the old keys of the strings of the game, "D. Kit" as in the Steam version)
			const char *attrib = "";

			if( cl_showplayerversion->value != 0.0f )
				attrib = gEngfuncs.PlayerInfo_ValueForKey( idx, "cscl_ver" );
			else if( !spectators && team )
			{
				if( extra->dead )
					attrib = LocalizeOrDefault( "#Cstrike_DEAD", "Dead" );
				else if( extra->has_c4 && seeBomb )
					attrib = LocalizeOrDefault( "#Cstrike_BOMB", "Bomb" );
				else if( extra->vip )
					attrib = LocalizeOrDefault( "#Cstrike_VIP", "VIP" );
				else if( extra->has_defuse_kit && seeKits )
					attrib = LocalizeOrDefault( "#Cstrike_DEFUSE_KIT", "D. Kit" );
			}

			paint.Text( tex[PT_ATTRIB], TextUtf::ToUtf8( attrib ? attrib : "" ), statusX, capTop, r, g, b, false,
						l.colRight[SbLayout::COL_SCORE] - statusX );

			// HP and Money: not for the spectators (neither in their rows nor in their table)
			std::string hp, money;

			if( moneyColumns && !spectators && extra->sb_health >= 0 && !extra->dead && gHUD.m_pShowHealth->value )
				hp = NumberText( extra->sb_health );

			if( moneyColumns && !spectators && extra->sb_account >= 0 && gHUD.m_pShowMoney->value )
				money = "$" + NumberText( extra->sb_account );

			paint.Text( tex[PT_HP], hp, l.colRight[SbLayout::COL_HP], capTop, r, g, b, true );
			paint.Text( tex[PT_MONEY], money, l.colRight[SbLayout::COL_MONEY], capTop, r, g, b, true );

			const bool scores = team && !spectators;
			paint.Text( tex[PT_SCORE], scores ? NumberText( extra->frags ) : "", l.colRight[SbLayout::COL_SCORE], capTop, r, g, b, true );
			paint.Text( tex[PT_DEATHS], scores ? NumberText( extra->deaths ) : "", l.colRight[SbLayout::COL_DEATHS], capTop, r, g, b, true );

			const char *bot;
			std::string ping;

			if( pl_info->ping <= 5  // must be 0, until Xash's bug not fixed
				&& ( bot = gEngfuncs.PlayerInfo_ValueForKey( idx, "*bot" )) && atoi( bot ) > 0 )
				ping = "BOT";
			else
				ping = NumberText( pl_info->ping );

			paint.Text( tex[PT_PING], ping, l.colRight[SbLayout::COL_PING], capTop, r, g, b, true );

			y += l.rowH;
		}

		return y;
	};

	if( !gHUD.m_Teamplay )
	{
		drawPlayers( NULL, false, sectionTop + l.teamRows );
		return true;
	}

	for( int section = 0; section < sections && !full; section++ )
	{
		team_info_t *team_info = &g_TeamInfo[order[section]];

		if( sectionTop + l.teamRows + l.rowH > l.bottom )
			break;

		int r, g, b;
		GetTeamColor( r, g, b, team_info->teamnumber );

		char numPlayers[16];
		snprintf( numPlayers, sizeof( numPlayers ), "%d", team_info->players );

		char fmtString[32];
		const char *fmtStringName = team_info->players == 1 ? "#Cstrike_ScoreBoard_Player" : "#Cstrike_ScoreBoard_Players";
		strncpy( fmtString, Localize( fmtStringName ), sizeof( fmtString ));
		fmtString[sizeof( fmtString ) - 1] = 0;

		if( !strcmp( fmtString, fmtStringName ))
		{
			const char *fallback = team_info->players == 1 ? "%s1    -   %s2 player" : "%s1    -   %s2 players";
			strncpy( fmtString, fallback, sizeof( fmtString ));
			fmtString[sizeof( fmtString ) - 1] = 0;
		}

		char teamName[64] = "";
		const bool isSpectators = team_info->teamnumber == TEAM_SPECTATOR || team_info->teamnumber == TEAM_UNASSIGNED;

		if( team_info->teamnumber == TEAM_TERRORIST || team_info->teamnumber == TEAM_CT )
		{
			const char *args[2] = { Localize( team_info->teamnumber == TEAM_CT ? "#Cstrike_ScoreBoard_CT" : "#Cstrike_ScoreBoard_Ter" ), numPlayers };
			Localize_Format( teamName, sizeof( teamName ), fmtString, args, 2 );
		}
		else if( isSpectators )
		{
			strncpy( teamName, Localize( "#Spectators" ), sizeof( teamName ));
			teamName[sizeof( teamName ) - 1] = 0;
		}
		else
		{
			strncpy( teamName, team_info->name, sizeof( teamName ));
			teamName[sizeof( teamName ) - 1] = 0;
		}

		CTextTexture *tex = g_sbTeam[section];
		const int capTop = sectionTop + l.teamCapTop;

		paint.Text( tex[TT_NAME], TextUtf::ToUtf8( teamName ), l.nameX, capTop, r, g, b, false,
					l.colRight[SbLayout::COL_HP] - l.nameX );
		paint.Text( tex[TT_SCORE], isSpectators ? "" : NumberText( team_info->frags ), l.colRight[SbLayout::COL_SCORE], capTop, r, g, b, true );
		// the average ping of a team only if it is above 0 (as in the Steam version), none for the spectators
		const int avgPing = team_info->players > 0 ? team_info->sumping / team_info->players : 0;
		paint.Text( tex[TT_PING], ( isSpectators || avgPing <= 0 ) ? "" : NumberText( avgPing ), l.colRight[SbLayout::COL_PING], capTop, r, g, b, true );

		// the line of the team: its colour, 1 pixel
		FillRGBABlend( l.lineX, sectionTop + l.teamLineY, l.lineW, 1, r, g, b, 255 );

		// the next team starts 2 pixels higher than the end of the rows (its capitals 15 pixels below them)
		sectionTop = drawPlayers( team_info->name, isSpectators, sectionTop + l.teamRows ) - 2;
	}

	// the players who are not in a team
	if( !full )
		drawPlayers( NULL, false, sections > 0 ? sectionTop + 2 : sectionTop + l.teamRows );

	return true;
}

// Y positions
#define ROW_GAP  15

int CHudScoreboard :: Draw( float flTime )
{
	if( !ShouldDrawScoreboard( ))
		return 1;

	float safe[4];
	GetSafeAreaRect( safe );

	if( m_bForceDraw )
	{
		xstart = safe[0] + safe[2] * m_flForceRect[0] + 0.5f;
		xend   = safe[0] + safe[2] * m_flForceRect[1] + 0.5f;
		ystart = safe[1] + safe[3] * m_flForceRect[2] + 0.5f;
		yend   = safe[1] + safe[3] * m_flForceRect[3] + 0.5f;
	}
	else
	{
		xstart     = 0.125f * ScreenWidth;
		xend       = ScreenWidth - xstart;
		ystart     = 90;
		yend       = ScreenHeight - ystart;
		m_colors.r = 0;
		m_colors.g = 0;
		m_colors.b = 0;
		m_colors.a = 153;
		m_bDrawStroke = true;

		// don't let the default layout go under a display cutout
		if( xstart < safe[0] ) xstart = safe[0];
		if( xend > safe[0] + safe[2] ) xend = safe[0] + safe[2];
		if( ystart < safe[1] ) ystart = safe[1];
		if( yend > safe[1] + safe[3] ) yend = safe[1] + safe[3];
	}

	return DrawScoreboard(flTime);
}

int CHudScoreboard :: DrawScoreboard( float fTime )
{
	GetAllPlayersInfo();

	if( !m_bForceDraw && DrawSteamStyle() )
		return 1;

	char ServerName[90];

//	Packetloss removed on Kelly 'shipping nazi' Bailey's orders
//	if ( cl_showpacketloss && cl_showpacketloss->value && ( ScreenWidth >= 400 ) )
//	{
//		can_show_packetloss = 1;
//	}

	// just sort the list on the fly
	// list is sorted first by frags, then by deaths
	float list_slot = 0;

	// calculate columns sizes
	g_Columns[COL_PING] = Column( xend - 15, Localize( "#PlayerPing" ) );
	g_Columns[COL_PING].end = min( g_Columns[COL_PING].end, g_Columns[COL_PING].start - DrawUtils::HudStringLen( "9999" ) );

	g_Columns[COL_DEATHS] = Column( g_Columns[COL_PING].end - 10, Localize( "#PlayerDeath" ) );
	g_Columns[COL_DEATHS].end = min( g_Columns[COL_DEATHS].end, g_Columns[COL_DEATHS].start - DrawUtils::HudStringLen( "9999" ) );

	g_Columns[COL_KILLS] = Column( g_Columns[COL_DEATHS].end - 10, Localize( "#PlayerScore" ) );
	g_Columns[COL_KILLS].end = min( g_Columns[COL_KILLS].end, g_Columns[COL_KILLS].start - DrawUtils::HudStringLen( "9999" ) );

	g_Columns[COL_MONEY] = Column( g_Columns[COL_KILLS].end - 10, LocalizeOrDefault( "#Cstrike_ACCOUNT", "Money" ) );
	g_Columns[COL_MONEY].end = min( g_Columns[COL_MONEY].end, g_Columns[COL_MONEY].start - DrawUtils::HudStringLen( "$999999" ) );

	g_Columns[COL_HP] = Column( g_Columns[COL_MONEY].end - 10, LocalizeOrDefault( "#Cstrike_HEALTH", "HP" ) );
	g_Columns[COL_HP].end = min( g_Columns[COL_HP].end, g_Columns[COL_HP].start - DrawUtils::HudStringLen( "999999" ) );

	g_Columns[COL_ATTRIB] = Column( g_Columns[COL_HP].end - 10 );
	g_Columns[COL_ATTRIB].end = g_Columns[COL_ATTRIB].start - DrawUtils::HudStringLen( "#Cstrike_DEFUSE_KIT" );

	g_Columns[COL_NAME] = Column( xstart + 15, nullptr, false );
	g_Columns[COL_NAME].end = g_Columns[COL_ATTRIB].end - 10;

	// print the heading line

	// the frame has rounded corners, the same as the MOTD window: 14 pixels on a big screen
	// (as measured on the Steam version), smaller on a small one, like in the Steam version
	int cornerRadius = (int)( 14.0f * min( 1.0f, ScreenHeight / 1017.0f ) + 0.5f );
	if( cornerRadius < 6 )
		cornerRadius = 6;

	DrawUtils::DrawRoundedRectangle(xstart, ystart, xend - xstart, yend - ystart, cornerRadius,
		m_colors.r, m_colors.g, m_colors.b, m_colors.a, m_bDrawStroke);

	int ypos = ystart + (list_slot * ROW_GAP) + 5;

	if( gHUD.m_szServerName[0] )
		// snprintf( ServerName, 80, "%s", (char*)(gHUD.m_Teamplay ? "TEAMS" : "PLAYERS"), gHUD.m_szServerName );
		strncpy( ServerName, gHUD.m_szServerName, 80 );
	else
		strncpy( ServerName, gHUD.m_Teamplay ? "TEAMS" : "PLAYERS", 80 );

	DrawUtils::DrawHudString( g_Columns[COL_NAME].start, ypos, g_Columns[COL_NAME].end, ServerName, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_HP].start, ypos, g_Columns[COL_HP].end, g_Columns[COL_HP].name, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_MONEY].start, ypos, g_Columns[COL_MONEY].end, g_Columns[COL_MONEY].name, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_KILLS].start, ypos, g_Columns[COL_KILLS].end, g_Columns[COL_KILLS].name, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_DEATHS].start, ypos, g_Columns[COL_DEATHS].end, g_Columns[COL_DEATHS].name, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_PING].start, ypos, g_Columns[COL_PING].end, g_Columns[COL_PING].name, 255, 140, 0 );

	list_slot += 2;
	ypos = ystart + (list_slot * ROW_GAP);
	FillRGBA( xstart, ypos, xend - xstart, 1, 255, 140, 0, 255);  // draw the separator line

	list_slot += 0.8;

	if ( gHUD.m_Teamplay )
	{
		DrawTeams( list_slot );
	}
	else
	{
		// it's not teamplay,  so just draw a simple player list
		DrawPlayers( list_slot );
	}
	return 1;
}

int CHudScoreboard :: DrawTeams( float list_slot )
{
	int j;
	int ypos = ystart + (list_slot * ROW_GAP) + 5;

	// clear out team scores
	for ( int i = 1; i <= m_iNumTeams; i++ )
	{
		if ( !g_TeamInfo[i].scores_overriden )
			g_TeamInfo[i].frags = g_TeamInfo[i].deaths = 0;
		g_TeamInfo[i].sumping = 0;
		g_TeamInfo[i].players = 0;
		g_TeamInfo[i].already_drawn = FALSE;
	}

	// recalc the team scores, then draw them
	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		if ( !g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0] )
			continue; // empty player slot, skip

		if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
			continue; // skip over players who are not in a team

		// find what team this player is in
		for ( j = 1; j <= m_iNumTeams; j++ )
		{
			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}

		if ( j > m_iNumTeams )  // player is not in a team, skip to the next guy
			continue;

		if ( !g_TeamInfo[j].scores_overriden )
		{
			g_TeamInfo[j].frags += g_PlayerExtraInfo[i].frags;
			g_TeamInfo[j].deaths += g_PlayerExtraInfo[i].deaths;
		}

		g_TeamInfo[j].sumping += g_PlayerInfoList[i].ping;

		if ( g_PlayerInfoList[i].thisplayer )
			g_TeamInfo[j].ownteam = TRUE;
		else
			g_TeamInfo[j].ownteam = FALSE;

		g_TeamInfo[j].players++;
	}

	// Draw the teams
	int iSpectatorPos = -1;

	while( true )
	{
		int highest_frags = -99999; int lowest_deaths = 99999;
		int best_team = 0;

		for ( int i = 1; i <= m_iNumTeams; i++ )
		{
			// don't draw team without players
			if ( g_TeamInfo[i].players <= 0 )
				continue;

			if (!strnicmp(g_TeamInfo[i].name, "SPECTATOR", MAX_TEAM_NAME))
			{
				iSpectatorPos = i;
				continue;
			}

			if ( !g_TeamInfo[i].already_drawn && g_TeamInfo[i].frags >= highest_frags )
			{
				if ( g_TeamInfo[i].frags > highest_frags || g_TeamInfo[i].deaths < lowest_deaths )
				{
					best_team = i;
					lowest_deaths = g_TeamInfo[i].deaths;
					highest_frags = g_TeamInfo[i].frags;
				}
			}
		}

		// draw the best team on the scoreboard
		if ( !best_team )
		{
			// if spectators is found and still not drawn
			if( iSpectatorPos != -1 && g_TeamInfo[iSpectatorPos].already_drawn == FALSE )
				best_team = iSpectatorPos;
			else break;
		}
		// draw out the best team
		team_info_t *team_info = &g_TeamInfo[best_team];

		// don't draw team without players
		if ( team_info->players <= 0 )
			continue;

		ypos = ystart + (list_slot * ROW_GAP);

		// check we haven't drawn too far down
		if ( ypos > yend )  // don't draw to close to the lower border
			break;

		int r, g, b;
		char teamName[64];

		char numPlayers[16];
		sprintf( numPlayers, "%d", team_info->players );

		char fmtString[32];
		const char *fmtStringName = team_info->players == 1 ? "#Cstrike_ScoreBoard_Player" : "#Cstrike_ScoreBoard_Players";
		strncpy( fmtString, Localize( fmtStringName ), sizeof( fmtString ) );
		fmtString[sizeof( fmtString ) - 1] = 0;

		if ( !strcmp( fmtString, fmtStringName ) )
		{
			const char *fallback = team_info->players == 1 ? "%s1    -   %s2 player" : "%s1    -   %s2 players";
			strncpy( fmtString, fallback, sizeof( fmtString ) );
			fmtString[sizeof( fmtString ) - 1] = 0;
		}

		GetTeamColor( r, g, b, team_info->teamnumber );
		switch ( team_info->teamnumber )
		{
		case TEAM_TERRORIST:
		{
			const char *args[2] = { Localize( "#Cstrike_ScoreBoard_Ter" ), numPlayers };
			Localize_Format( teamName, sizeof( teamName ), fmtString, args, 2 );
			DrawUtils::DrawHudNumberString( g_Columns[COL_KILLS].start, ypos, g_Columns[COL_KILLS].end, team_info->frags, r, g, b );
			break;
		}
		case TEAM_CT:
		{
			const char *args[2] = { Localize( "#Cstrike_ScoreBoard_CT" ), numPlayers };
			Localize_Format( teamName, sizeof( teamName ), fmtString, args, 2 );
			DrawUtils::DrawHudNumberString( g_Columns[COL_KILLS].start, ypos, g_Columns[COL_KILLS].end, team_info->frags, r, g, b );
			break;
		}
		case TEAM_SPECTATOR:
		case TEAM_UNASSIGNED:
			strncpy( teamName, Localize( "#Spectators" ), sizeof( teamName ) );
			break;
		}

		DrawUtils::DrawHudString( g_Columns[COL_NAME].start, ypos, g_Columns[COL_NAME].end, teamName, r, g, b );
		DrawUtils::DrawHudNumberString( g_Columns[COL_PING].start, ypos, g_Columns[COL_PING].end, team_info->sumping / team_info->players, r, g, b );

		team_info->already_drawn = TRUE;  // set the already_drawn to be TRUE, so this team won't get drawn again

		// draw underline
		list_slot += 1.2f;
		FillRGBA( xstart, ystart + (list_slot * ROW_GAP), xend - xstart, 1, r, g, b, 255);

		list_slot += 0.4f;
		// draw all the players that belong to this team, indented slightly
		list_slot = DrawPlayers( list_slot, 10, team_info->name );
	}

	// draw all the players who are not in a team
	list_slot += 4.0f;
	DrawPlayers( list_slot, 0, "" );

	return 1;
}

// returns the ypos where it finishes drawing
int CHudScoreboard :: DrawPlayers( float list_slot, int nameoffset, const char *team )
{
	// draw the players, in order,  and restricted to team if set
	while ( 1 )
	{
		// Find the top ranking player
		int highest_frags = -99999;	int lowest_deaths = 99999;
		int best_player = 0;

		for ( int i = 1; i < MAX_PLAYERS; i++ )
		{
			if ( g_PlayerInfoList[i].name && g_PlayerExtraInfo[i].frags >= highest_frags )
			{
				if ( !(team && stricmp(g_PlayerExtraInfo[i].teamname, team)) )  // make sure it is the specified team
				{
					extra_player_info_t *pl_info = &g_PlayerExtraInfo[i];
					if ( pl_info->frags > highest_frags || pl_info->deaths < lowest_deaths )
					{
						best_player = i;
						lowest_deaths = pl_info->deaths;
						highest_frags = pl_info->frags;
					}
				}
			}
		}

		if ( !best_player )
			break;

		// draw out the best player
		hud_player_info_t *pl_info = &g_PlayerInfoList[best_player];

		int ypos = ystart + (list_slot * ROW_GAP);

		// check we haven't drawn too far down
		if ( ypos > yend )  // don't draw to close to the lower border
			break;

		int r = 255, g = 255, b = 255;
		float *colors = GetClientColor( best_player );
		r *= colors[0];
		g *= colors[1];
		b *= colors[2];

		if(pl_info->thisplayer) // hey, it's me!
		{
			FillRGBABlend( xstart, ypos, xend - xstart, ROW_GAP, 255, 255, 255, 15 );
		}

		DrawUtils::DrawHudString( g_Columns[COL_NAME].start + nameoffset, ypos, g_Columns[COL_NAME].start + 350, pl_info->name, r, g, b );

		if( cl_showplayerversion->value == 0.0f )
		{
			if( team && stricmp( team, "SPECTATOR" ))
			{
				// draw bomb( if player have the bomb )
				if( g_PlayerExtraInfo[best_player].dead )
					DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, Localize( "#Cstrike_DEAD" ), r, g, b );
				else if( g_PlayerExtraInfo[best_player].has_c4 )
					DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, Localize( "#Cstrike_BOMB" ), r, g, b );
				else if( g_PlayerExtraInfo[best_player].vip )
					DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, Localize( "#Cstrike_VIP" ),  r, g, b );
				else if (g_PlayerExtraInfo[best_player].has_defuse_kit )
					DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, Localize( "#Cstrike_DEFUSE_KIT" ),  r, g, b );
			}
		}
		else
		{
			DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, gEngfuncs.PlayerInfo_ValueForKey( best_player, "cscl_ver" ),  r, g, b );
		}

		if ( g_PlayerExtraInfo[best_player].sb_health >= 0 && !g_PlayerExtraInfo[best_player].dead )
		{
			if ( gHUD.m_pShowHealth->value )
			{
				static char buf[64];
				sprintf( buf, "%d", g_PlayerExtraInfo[best_player].sb_health );
				DrawUtils::DrawHudStringReverse( g_Columns[COL_HP].start, ypos, g_Columns[COL_HP].end, buf, r, g, b );
			}
		}

		if ( g_PlayerExtraInfo[best_player].sb_account >= 0 )
		{
			if ( gHUD.m_pShowMoney->value )
			{
				static char buf[64];
				sprintf( buf, "$%d", g_PlayerExtraInfo[best_player].sb_account );
				DrawUtils::DrawHudStringReverse( g_Columns[COL_MONEY].start, ypos, g_Columns[COL_MONEY].end, buf, r, g, b );
			}
		}

		// draw kills (right to left)
		if( team && stricmp( team, "SPECTATOR" ) )
		{
			DrawUtils::DrawHudNumberString( g_Columns[COL_KILLS].start, ypos, g_Columns[COL_KILLS].end, g_PlayerExtraInfo[best_player].frags, r, g, b );

			// draw deaths
			DrawUtils::DrawHudNumberString( g_Columns[COL_DEATHS].start, ypos, g_Columns[COL_DEATHS].end, g_PlayerExtraInfo[best_player].deaths, r, g, b );
		}

		// draw ping & packetloss
		const char *value;
		if( pl_info->ping <= 5  // must be 0, until Xash's bug not fixed
			&& ( value = gEngfuncs.PlayerInfo_ValueForKey( best_player, "*bot" ) )
			&& atoi( value ) > 0 )
		{
			DrawUtils::DrawHudStringReverse( g_Columns[COL_PING].start, ypos, g_Columns[COL_PING].end, "BOT", r, g, b );
		}
		else
		{
			static char buf[64];
			sprintf( buf, "%d", pl_info->ping );
			DrawUtils::DrawHudStringReverse( g_Columns[COL_PING].start, ypos, g_Columns[COL_PING].end, buf, r, g, b );
		}

		pl_info->name = NULL;  // set the name to be NULL, so this client won't get drawn again
		list_slot++;
	}

	list_slot += 2.0f;

	return list_slot;
}


void CHudScoreboard :: GetAllPlayersInfo( void )
{
	memset( &g_PlayerInfoList[0], 0, sizeof( g_PlayerInfoList[0] ));

	for( int i = 1; i < MAX_PLAYERS; i++ )
		GetPlayerInfo( i, &g_PlayerInfoList[i] );
}

int CHudScoreboard :: MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf )
{
	m_iFlags |= HUD_DRAW;

	BufferReader reader( pszName, pbuf, iSize );
	short cl = reader.ReadByte();
	short frags = reader.ReadShort();
	short deaths = reader.ReadShort();
	short playerclass = reader.ReadShort();
	reader.ReadShort();

	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		g_PlayerExtraInfo[cl].frags = frags;
		g_PlayerExtraInfo[cl].deaths = deaths;
		g_PlayerExtraInfo[cl].playerclass = playerclass;

		//gViewPort->UpdateOnPlayerInfo();
	}

	return 1;
}

// Message handler for TeamInfo message
// accepts two values:
//		byte: client number
//		string: client team name
int CHudScoreboard :: MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );
	short cl = reader.ReadByte();
	int teamNumber = 0;

	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		// set the players team
		char teamName[MAX_TEAM_NAME];
		strncpy( teamName, reader.ReadString(), MAX_TEAM_NAME );
		teamName[MAX_TEAM_NAME-1] = 0;

		if( !strcmp( teamName, "TERRORIST") )
			teamNumber = TEAM_TERRORIST;
		else if( !strcmp( teamName, "CT") )
			teamNumber = TEAM_CT;
		else if( !strcmp( teamName, "SPECTATOR" ) )
		{
			teamNumber = TEAM_SPECTATOR;
		}
		else if( !strcmp( teamName, "UNASSIGNED" ) )
		{
			teamNumber = TEAM_UNASSIGNED;
			strncpy( teamName, "SPECTATOR", MAX_TEAM_NAME );
		}
		// just in case
		else teamNumber = TEAM_UNASSIGNED;

		strncpy( g_PlayerExtraInfo[cl].teamname, teamName, MAX_TEAM_NAME );
		g_PlayerExtraInfo[cl].teamnumber = teamNumber;
	}

	RebuildTeams();
	return 1;
}

// the list of teams from the teams of the players
void CHudScoreboard :: RebuildTeams( void )
{

	// clear out player counts from teams
	for ( int i = 1; i <= m_iNumTeams; i++ )
	{
		g_TeamInfo[i].players = 0;
	}

	// rebuild the team list
	GetAllPlayersInfo();
	m_iNumTeams = 0;

	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		int j;
		//if ( g_PlayerInfoList[i].name == NULL )
		//	continue;

		if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
			continue; // skip over players who are not in a team

		// is this player in an existing team?
		for ( j = 1; j <= m_iNumTeams; j++ )
		{
			if ( g_TeamInfo[j].name[0] == '\0' )
				break;

			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}

		if ( j > m_iNumTeams )
		{
			// they aren't in a listed team, so make a new one
			for ( j = 1; j <= m_iNumTeams; j++ )
			{
				if ( g_TeamInfo[j].name[0] == '\0' )
					break;
			}


			m_iNumTeams = max( j, m_iNumTeams );

			strncpy( g_TeamInfo[j].name, g_PlayerExtraInfo[i].teamname, MAX_TEAM_NAME );
			g_TeamInfo[j].teamnumber = g_PlayerExtraInfo[i].teamnumber;
			g_TeamInfo[j].players = 0;
		}

		g_TeamInfo[j].players++;
	}

	// clear out any empty teams
	for ( int i = 1; i <= m_iNumTeams; i++ )
	{
		if ( g_TeamInfo[i].players < 1 )
			memset( &g_TeamInfo[i], 0, sizeof(team_info_t) );
	}
}

// Message handler for TeamScore message
// accepts three values:
//		string: team name
//		short: teams kills
//		short: teams deaths
// if this message is never received, then scores will simply be the combined totals of the players.
int CHudScoreboard :: MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );
	char *TeamName = reader.ReadString();
	int i;

	// find the team matching the name
	for ( i = 0; i < m_iNumTeams; i++ )
	{
		if ( !stricmp( TeamName, g_TeamInfo[i].name ) )
			break;
	}
	if ( i > m_iNumTeams )
	{
		reader.Flush();
		return 1;
	}

	// use this new score data instead of combined player scores
	g_TeamInfo[i].scores_overriden = TRUE;
	g_TeamInfo[i].frags = reader.ReadShort();
	// g_TeamInfo[i].deaths = reader.ReadShort();

	return 1;
}

void CHudScoreboard :: DeathMsg( int killer, int victim )
{
	// if we were the one killed,  or the world killed us, set the scoreboard to indicate suicide
	if ( victim == m_iPlayerNum || killer == 0 )
	{
		m_iLastKilledBy = killer ? killer : m_iPlayerNum;
		m_fLastKillTime = gHUD.m_flTime + 10;	// display who we were killed by for 10 seconds

		if ( killer == m_iPlayerNum )
			m_iLastKilledBy = m_iPlayerNum;
	}
}



void CHudScoreboard :: UserCmd_ShowScores( void )
{
	m_bForceDraw = false;
	m_bShowscoresHeld = true;
}

void CHudScoreboard :: UserCmd_HideScores( void )
{
	m_bForceDraw = m_bShowscoresHeld = false;
}


void CHudScoreboard	:: UserCmd_ShowScoreboard2()
{
	if( gEngfuncs.Cmd_Argc() != 9 )
	{
		ConsolePrint("showscoreboard2 <xstart> <xend> <ystart> <yend> <r> <g> <b> <a>");
	}

	// only remember fractions here, pixels are calculated in Draw()
	m_flForceRect[0] = atof(gEngfuncs.Cmd_Argv(1));
	m_flForceRect[1] = atof(gEngfuncs.Cmd_Argv(2));
	m_flForceRect[2] = atof(gEngfuncs.Cmd_Argv(3));
	m_flForceRect[3] = atof(gEngfuncs.Cmd_Argv(4));
	m_colors.r = atoi(gEngfuncs.Cmd_Argv(5));
	m_colors.g = atoi(gEngfuncs.Cmd_Argv(6));
	m_colors.b = atoi(gEngfuncs.Cmd_Argv(7));
	m_colors.a = atoi(gEngfuncs.Cmd_Argv(8));
	m_bDrawStroke = false;
	m_bForceDraw = true;
}

void CHudScoreboard :: UserCmd_HideScoreboard2()
{
	m_bForceDraw = m_bShowscoresHeld = false; // and disable it
}
