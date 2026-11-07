//
// tutor.cpp
//
// Tutor messages of the server: TutorText shows a message in the upper right corner of the screen,
// TutorClose hides it (TutorState and TutorLine are read and not used, as in the Steam version of CS 1.6).
// They are sent by ReGameDLL (the tutor of Condition Zero) and by many AMX Mod X plugins.
//
// The window repeats the one of the Steam version (see tutor_logic.h for the sizes): one line of text,
// the window gets as wide as the text is. Its colours and font
// are taken from resource/TutorScheme.res (servers send this file with the plugins), the icons from
// gfx/career/icon_i.tga and icon_skulls.tga. Without these files the window is drawn by the client itself.
//
// cl_tutor 0          - do not show the messages
// cl_tutor_maxtime N  - hide a message after N seconds even if the server does not close it (0 - never)
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "draw_util.h"
#include "vgui_parser.h"

#include <stdio.h>
#include <string>
#include <vector>

#include "tutor_logic.h"
#include "client_texture.h"
#include "text_utf8.h"

static const char *const s_iconFiles[Tutor::ICON_COUNT] =
{
	"gfx/career/icon_i.tga",
	"gfx/career/icon_skulls.tga",
};

int CHudTutor::Init( void )
{
	HOOK_MESSAGE( gHUD.m_Tutor, TutorText );
	HOOK_MESSAGE( gHUD.m_Tutor, TutorClose );
	HOOK_MESSAGE( gHUD.m_Tutor, TutorState );
	HOOK_MESSAGE( gHUD.m_Tutor, TutorLine );

	cl_tutor = CVAR_CREATE( "cl_tutor", "1", FCVAR_ARCHIVE );
	cl_tutor_maxtime = CVAR_CREATE( "cl_tutor_maxtime", "30", FCVAR_ARCHIVE );

	m_bShow = false;
	m_iType = Tutor::TYPE_DEFAULT;
	m_flShownAt = 0.0f;
	m_bTextAtlas = false;
	m_iTextW = m_iTextH = 0;
	m_bTextDirty = false;
	m_bSchemeLoaded = false;

	for( int i = 0; i < Tutor::ICON_COUNT; ++i )
	{
		m_iIconTex[i] = 0;
		m_bIconTried[i] = false;
	}

	m_iFlags = HUD_DRAW | HUD_INTERMISSION;
	gHUD.AddHudElem( this );
	return 1;
}

int CHudTutor::VidInit( void )
{
	// a new map: the files of the server (scheme, icons) may be other ones
	Hide();
	FreeText();

	m_scheme = Tutor::Scheme();
	m_bSchemeLoaded = false;

	for( int i = 0; i < Tutor::ICON_COUNT; ++i )
	{
		m_iIconTex[i] = 0;
		m_bIconTried[i] = false;
	}

	return 1;
}

void CHudTutor::InitHUDData( void )
{
	Hide();
}

void CHudTutor::Hide( void )
{
	m_bShow = false;
	m_szText.clear();
}

void CHudTutor::FreeText( void )
{
	m_text.Free();
	m_bTextAtlas = false;
	m_iTextW = m_iTextH = 0;
	m_lines.clear();
	m_bTextDirty = true;
}

// resource/TutorScheme.res and the icons are read when the first message of a map comes:
// by then the server has sent them
void CHudTutor::LoadResources( void )
{
	if( !m_bSchemeLoaded )
	{
		m_bSchemeLoaded = true;
		m_scheme = Tutor::Scheme();

		int len = 0;
		byte *data = gEngfuncs.COM_LoadFile( "resource/TutorScheme.res", 5, &len );

		if( data )
		{
			const std::string text( (const char *)data, len > 0 ? (size_t)len : 0 );
			gEngfuncs.COM_FreeFile( data );
			Tutor::ParseScheme( text.c_str(), m_scheme );
		}
	}

	for( int i = 0; i < Tutor::ICON_COUNT; ++i )
	{
		if( m_iIconTex[i] > 0 || m_bIconTried[i] || !gRenderAPI.GL_LoadTexture )
			continue;

		m_bIconTried[i] = true;

		// the engine prints nothing if there is no such file: the client then draws the icon itself
		if( gEngfuncs.COM_LoadFile )
		{
			int len = 0;
			byte *probe = gEngfuncs.COM_LoadFile( s_iconFiles[i], 5, &len );

			if( !probe )
				continue;

			gEngfuncs.COM_FreeFile( probe );
		}

		m_iIconTex[i] = gRenderAPI.GL_LoadTexture( s_iconFiles[i], NULL, 0, TF_NOMIPMAP | TF_CLAMP );
	}
}

int CHudTutor::MsgFunc_TutorText( const char *pszName, int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );

	// the strings are copied at once: the reader gives all of them in one buffer
	const std::string raw = reader.ReadString();
	const int numArgs = reader.ReadByte();
	std::vector<std::string> args;

	for( int i = 0; i < numArgs; ++i )
	{
		const std::string arg = reader.ReadString();

		if( i < 9 )
			args.push_back( TextUtf::ToUtf8( arg.c_str() ));
	}

	reader.ReadShort(); // number of the message in tutordata.txt
	reader.ReadShort(); // the player is dead
	const int type = reader.ReadShort();

	// "#Key" is a string of the game (resource/cstrike_<language>.txt)
	const char *text = raw.c_str();

	if( text[0] == '#' )
		text = Localize( text );

	const std::string message = Tutor::CleanText( Tutor::Substitute( TextUtf::ToUtf8( text ), args ));

	if( message.empty() )
	{
		Hide();
		return 1;
	}

	LoadResources();

	if( message != m_szText )
	{
		m_szText = message;
		FreeText();
	}

	m_iType = type > 0 ? type : Tutor::TYPE_DEFAULT;
	m_flShownAt = gHUD.m_flTime;
	m_bShow = true;

	if( gEngfuncs.pfnGetCvarFloat( "developer" ) > 0 )
		gEngfuncs.Con_Printf( "[tutor] type %d: %s\n", m_iType, m_szText.c_str() );

	return 1;
}

int CHudTutor::MsgFunc_TutorClose( const char *pszName, int iSize, void *pbuf )
{
	Hide();
	return 1;
}

int CHudTutor::MsgFunc_TutorState( const char *pszName, int iSize, void *pbuf )
{
	// the state of the tutor of Condition Zero, not shown in CS 1.6
	BufferReader reader( pszName, pbuf, iSize );
	reader.ReadString();
	return 1;
}

int CHudTutor::MsgFunc_TutorLine( const char *pszName, int iSize, void *pbuf )
{
	// a line to an entity, not used by the game
	return 1;
}

// The picture of the text (Windows fonts) or its lines for the font of the HUD; maxWidth - the widest line
void CHudTutor::BuildText( int maxWidth )
{
	m_bTextDirty = false;

	// the text is in screen pixels, the layout is in the units of the HUD (the same unless hud_scale is used)
	const float scale = gHUD.m_flScale > 0.0f ? gHUD.m_flScale : 1.0f;

	if( m_text.Update( m_szText, m_scheme.fontName.c_str(), (int)( Tutor::FontEmHeight( m_scheme.fontTall ) * scale + 0.5f ),
			m_scheme.fontWeight, m_scheme.fontAntialias, (int)( maxWidth * scale )) && m_text.Valid() )
	{
		m_bTextAtlas = true;
		m_iTextW = m_text.Width();
		m_iTextH = m_text.Height();
		return;
	}
	m_bTextAtlas = false;

	// no Windows fonts: the font of the HUD
	m_lines = Tutor::ClipLines( m_szText, maxWidth, []( const std::string &s ) { return DrawUtils::HudStringLen( s.c_str() ); } );
	m_iTextW = 0;

	for( const std::string &line : m_lines )
	{
		const int lw = DrawUtils::HudStringLen( line.c_str() );
		if( lw > m_iTextW )
			m_iTextW = lw;
	}

	m_iTextH = (int)m_lines.size() * gHUD.GetCharHeight();
}

static void DrawTexture( int tex, float x, float y, float w, float h, const Tutor::Color &c, float scale )
{
	gRenderAPI.GL_SelectTexture( 0 );
	gRenderAPI.GL_Bind( 0, tex );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f );
	DrawUtils::Draw2DQuad( x * scale, y * scale, ( x + w ) * scale, ( y + h ) * scale );
}

// The icon drawn by the client when the server did not send gfx/career/*.tga:
// a rounded square of the colour of the message with a dark sign on it
static void DrawOwnIcon( int x, int y, Tutor::Icon icon, const Tutor::Color &c )
{
	const int s = Tutor::Size::IconSize - 2;
	x += 1;
	y += 1;

	DrawUtils::FillRoundedRect( x, y, s, s, 3, true, true, c.r, c.g, c.b, 255 );

	if( icon == Tutor::ICON_SKULL )
	{
		// a cross
		const int n = s - 12;

		for( int i = 0; i < n; ++i )
		{
			FillRGBA( x + 6 + i, y + 6 + i, 2, 2, 20, 20, 20, 255 );
			FillRGBA( x + s - 8 - i, y + 6 + i, 2, 2, 20, 20, 20, 255 );
		}
	}
	else
	{
		// "i"
		const int cx = x + s / 2;
		FillRGBA( cx - 2, y + 4, 4, 3, 20, 20, 20, 255 );
		FillRGBA( cx - 2, y + 9, 4, s - 13, 20, 20, 20, 255 );
	}
}

int CHudTutor::Draw( float flTime )
{
	if( !m_bShow || m_szText.empty() )
		return 1;

	if( cl_tutor && cl_tutor->value == 0.0f )
		return 1;

	// the server forgot to close it (or its TutorClose was lost)
	if( cl_tutor_maxtime && cl_tutor_maxtime->value > 0.0f && flTime - m_flShownAt > cl_tutor_maxtime->value )
	{
		Hide();
		return 1;
	}

	// a new map started: the time of the client starts from zero again
	if( flTime < m_flShownAt )
		m_flShownAt = flTime;

	const float scale = gHUD.m_flScale > 0.0f ? gHUD.m_flScale : 1.0f;

	if( m_bTextDirty )
		BuildText( Tutor::MaxTextWidth( ScreenWidth ));

	// in units of the HUD
	const int textW = m_bTextAtlas ? (int)( m_iTextW / scale + 0.5f ) : m_iTextW;
	const int textH = m_bTextAtlas ? (int)( m_iTextH / scale + 0.5f ) : m_iTextH;

	// in the spectator mode the window is below the upper bar of the spectator
	int top = 0;
	if( g_iUser1 && gHUD.GetGameType() != GAME_CZERODS )
		top = CHudSpectatorGui::BarHeight();

	const Tutor::Layout l = Tutor::ComputeLayout( ScreenWidth, top, textW, textH );
	const Tutor::Color bg = m_scheme.background;
	const Tutor::Color color = m_scheme.ColorForType( m_iType );
	const Tutor::Icon icon = Tutor::IconForType( m_iType );

	// the panel: the part with the icon and the part with the text, the game is seen through the cut between them
	const int rightX = l.cutX + Tutor::Size::Cut;
	DrawUtils::FillRoundedRectSides( l.x, l.y, l.cutX - l.x, l.h, Tutor::Size::Radius, true, false, bg.r, bg.g, bg.b, bg.a );
	DrawUtils::FillRoundedRectSides( rightX, l.y, l.x + l.w - rightX, l.h, Tutor::Size::Radius, false, true, bg.r, bg.g, bg.b, bg.a );

	// the icon: a white picture painted with the colour of the message
	if( m_iIconTex[icon] > 0 )
	{
		const Tutor::Color opaque = { color.r, color.g, color.b, 255 };
		DrawTexture( m_iIconTex[icon], (float)l.iconX, (float)l.iconY, (float)Tutor::Size::IconSize, (float)Tutor::Size::IconSize, opaque, scale );
	}
	else
	{
		DrawOwnIcon( l.iconX, l.iconY, icon, color );
	}

	// the text
	if( m_bTextAtlas )
	{
		m_text.Draw( l.textX * scale, l.textY * scale, color.r, color.g, color.b, color.a );
	}
	else
	{
		int y = l.textY;

		for( const std::string &line : m_lines )
		{
			DrawUtils::DrawHudString( l.textX, y, ScreenWidth, line.c_str(), color.r, color.g, color.b );
			y += gHUD.GetCharHeight();
		}
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	return 1;
}
