/*
draw_util.cpp - Draw Utils
Copyright (C) 2016 a1batross

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

#include "hud.h"
#include "cl_util.h"
#include "draw_util.h"
#include "triangleapi.h"
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "utflib.h"
#include "utlvector.h"

float DrawUtils::color[3];

#define IsColorString( p )	( p && *( p ) == '^' && *(( p ) + 1) && *(( p ) + 1) >= '0' && *(( p ) + 1 ) <= '9' )
#define ColorIndex( c )	((( c ) - '0' ) & 7 )

// console color typeing
byte g_color_table[][4] =
{
{100, 100, 100, 255},	// should be black, but hud font is additive, so printing black characters is impossible
{255,   0,   0, 255},	// red
{  0, 255,   0, 255},	// green
{255, 255,   0, 255},	// yellow
{  0,   0, 255, 255},	// blue
{  0, 255, 255, 255},	// cyan
{255,   0, 255, 255},	// magenta
{240, 180,  24, 255},	// default color (can be changed by user)
};

int g_codepage = 0;
qboolean g_accept_utf8;

cvar_t *con_charset;
cvar_t *cl_charset;

/*
============================
Con_UtfProcessChar

Convert utf char to current font's single-byte encoding
============================
*/
int Con_UtfProcessCharForce( int in )
{
	// TODO: get rid of global state where possible
	static utfstate_t state = { 0 };

	uint32_t ch = Q_DecodeUTF8( &state, in );

	if( g_codepage == 1251 )
		return Q_UnicodeToCP1251( ch );
	if( g_codepage == 1252 )
		return Q_UnicodeToCP1252( ch );

	return '?'; // not implemented yet
}

int Con_UtfProcessChar( int in )
{
	if( !g_accept_utf8 ) // incoming character is not a UTF-8 sequence
		return in;

	// otherwise, decode it and convert to selected codepage
	return Con_UtfProcessCharForce( in );
}

int DrawUtils::DrawHudString( int xpos, int ypos, int iMaxX, const char *str, int r, int g, int b, float scale )
{
	int first_xpos = xpos;
	char *szIt = (char *)str;

	Con_UtfProcessChar( 0 );

	// draw the string until we hit the null character or a newline character
	for ( ; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		if ( *szIt == '\\' && *( szIt + 1 ) != '\n' && *( szIt + 1 ) != 0 )
		{
			// an escape character

			switch ( *( ++szIt ) )
			{
			case 'y':
				UnpackRGB( r, g, b, RGB_YELLOWISH );
				continue;
			case 'r':
				UnpackRGB( r, g, b, RGB_REDISH );
				continue;
			case 'w':
				UnpackRGB( r, g, b, RGB_WHITE );
				continue;
			case 'd':
				UnpackRGB( r, g, b, RGB_GRAY );
				continue;
			case 'R':
				xpos = iMaxX - gHUD.GetCharWidth( 'M' ) * 10;
				++szIt;
			}
		}
		else if( IsColorString( szIt ) )
		{
			szIt++;
			if( gHUD.hud_colored->value )
			{
				r = g_color_table[ColorIndex( *szIt )][0];
				g = g_color_table[ColorIndex( *szIt )][1];
				b = g_color_table[ColorIndex( *szIt )][2];
			}
			continue;
		}

		int uch = Con_UtfProcessChar( (unsigned char)*szIt );

		int next = xpos + gHUD.GetCharWidth( uch ); // variable-width fonts look cool

		if ( next > iMaxX && iMaxX > 0 )
			return xpos;

		xpos += TextMessageDrawChar( xpos, ypos, ( unsigned char )*szIt, r, g, b, scale );
	}

	return xpos;
}


int DrawUtils::DrawHudStringReverse( int xpos, int ypos, int iMinX, const char *szString, int r, int g, int b, float scale )
{
	int first_xpos = xpos;

	struct CharEntry {
		int start, end;
		int uch;
		int r, g, b;
	};
	CUtlVector<CharEntry> chars;

	int curR = r, curG = g, curB = b;
	Con_UtfProcessChar( 0 );

	// 1. Forward pass to identify characters and their correct colors
	for ( int i = 0; szString[i] != '\0'; )
	{
		if ( szString[i] == '\\' && szString[i + 1] != 0 && szString[i + 1] != '\n' )
		{
			switch ( szString[i + 1] )
			{
			case 'y': UnpackRGB( curR, curG, curB, RGB_YELLOWISH ); break;
			case 'r': UnpackRGB( curR, curG, curB, RGB_REDISH ); break;
			case 'w': UnpackRGB( curR, curG, curB, RGB_WHITE ); break;
			case 'd': UnpackRGB( curR, curG, curB, RGB_GRAY ); break;
			case 'R':
				// Draw any characters collected so far (to the left of \R)
				for ( int j = chars.Count() - 1; j >= 0; j-- )
				{
					int width = gHUD.GetCharWidth( chars[j].uch );
					int next = xpos - width;
					if ( next < iMinX ) break;
					xpos = next;
					int drawX = xpos;
					for ( int k = chars[j].start; k <= chars[j].end; k++ )
						drawX += TextMessageDrawChar( drawX, ypos, ( unsigned char )szString[k], chars[j].r, chars[j].g, chars[j].b, scale );
				}
				chars.Purge();
				// Draw the rest of the string forward from the left
				return DrawHudString( iMinX, ypos, first_xpos, &szString[i+2], curR, curG, curB, scale );
			}
			i += 2;
			continue;
		}
		else if ( IsColorString( &szString[i] ) )
		{
			if ( gHUD.hud_colored->value )
			{
				curR = g_color_table[ColorIndex( szString[i + 1] )][0];
				curG = g_color_table[ColorIndex( szString[i + 1] )][1];
				curB = g_color_table[ColorIndex( szString[i + 1] )][2];
			}
			i += 2;
			continue;
		}

		// Identify the character boundary
		int start = i;
		int next_i = i + 1;
		if ( g_accept_utf8 )
		{
			while( szString[next_i] != '\0' && (unsigned char)szString[next_i] >= 0x80 && (unsigned char)szString[next_i] <= 0xBF )
				next_i++;
		}

		// Decode the character to get its width-code
		Con_UtfProcessChar( 0 );
		int uch = 0;
		for ( int j = start; j < next_i; j++ )
			uch = Con_UtfProcessChar( (unsigned char)szString[j] );

		if ( uch )
		{
			CharEntry entry;
			entry.start = start;
			entry.end = next_i - 1;
			entry.uch = uch;
			entry.r = curR;
			entry.g = curG;
			entry.b = curB;
			chars.AddToTail( entry );
		}
		i = next_i;
	}

	// 2. Backward pass to draw collected characters
	for ( int i = chars.Count() - 1; i >= 0; i-- )
	{
		int width = gHUD.GetCharWidth( chars[i].uch );
		int next = xpos - width;

		if ( next < iMinX )
			return xpos;

		xpos = next;

		// Draw all bytes of this character in forward order
		int drawX = xpos;
		for ( int j = chars[i].start; j <= chars[i].end; j++ )
			drawX += TextMessageDrawChar( drawX, ypos, ( unsigned char )szString[j], chars[i].r, chars[i].g, chars[i].b, scale );
	}

	return xpos;
}

int DrawUtils::DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b )
{
	int iWidth = gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).Width();
	int k;

	if ( iNumber > 0 )
	{
		// SPR_Draw 100's
		if ( iNumber >= 100 )
		{
			k = iNumber / 100;
			SPR_Set( gHUD.GetSprite( gHUD.m_HUD_number_0 + k ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( gHUD.m_HUD_number_0 + k ) );
			x += iWidth;
		}
		else if ( iFlags & ( DHN_3DIGITS ) )
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 10's
		if ( iNumber >= 10 )
		{
			k = ( iNumber % 100 ) / 10;
			SPR_Set( gHUD.GetSprite( gHUD.m_HUD_number_0 + k ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( gHUD.m_HUD_number_0 + k ) );
			x += iWidth;
		}
		else if ( iFlags & ( DHN_3DIGITS | DHN_2DIGITS ) )
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set( gHUD.GetSprite( gHUD.m_HUD_number_0 + k ), r, g, b );
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( gHUD.m_HUD_number_0 + k ) );
		x += iWidth;
	}
	else if ( iFlags & DHN_DRAWZERO )
	{
		SPR_Set( gHUD.GetSprite( gHUD.m_HUD_number_0 ), r, g, b );

		// SPR_Draw 100's
		if ( iFlags & ( DHN_3DIGITS ) )
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		if ( iFlags & ( DHN_3DIGITS | DHN_2DIGITS ) )
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones

		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ) );
		x += iWidth;
	}

	return x;
}

int DrawUtils::DrawHudNumber2( int x, int y, bool DrawZero, int iDigits, int iNumber, int r, int g, int b )
{
	// Negative remainders would produce invalid digit sprite indices
	if( iNumber < 0 )
		iNumber = 0;

	int iWidth = gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).Width();
	x += ( iDigits - 1 ) * iWidth;

	int ResX = x + iWidth;
	do
	{
		int k = iNumber % 10;
		iNumber /= 10;
		SPR_Set( gHUD.GetSprite( gHUD.m_HUD_number_0 + k ), r, g, b );
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( gHUD.m_HUD_number_0 + k ) );
		x -= iWidth;
		iDigits--;
	} while ( iNumber > 0 || ( iDigits > 0 && DrawZero ) );

	return ResX;
}

int DrawUtils::DrawHudNumber2( int x, int y, int iNumber, int r, int g, int b )
{
	// Negative remainders would produce invalid digit sprite indices
	if( iNumber < 0 )
		iNumber = 0;

	int iWidth = gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).Width();

	int iDigits = 0;
	int temp    = iNumber;
	do
	{
		iDigits++;
		temp /= 10;
	} while ( temp > 0 );

	x += ( iDigits - 1 ) * iWidth;

	int ResX = x + iWidth;
	do
	{
		int k = iNumber % 10;
		iNumber /= 10;
		SPR_Set( gHUD.GetSprite( gHUD.m_HUD_number_0 + k ), r, g, b );
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( gHUD.m_HUD_number_0 + k ) );
		x -= iWidth;
	} while ( iNumber > 0 );

	return ResX;
}

// How far the rounded corner is from the side of the rectangle in the row i (0 is the outermost row)
static int RoundedInset( int radius, int i )
{
	const float dy = radius - i - 0.5f;
	return radius - (int)( sqrtf( (float)( radius * radius ) - dy * dy ) + 0.5f );
}

// Blended rectangle made of 1 pixel rows; the upper and/or the lower corners are rounded
void DrawUtils::FillRoundedRect( int x, int y, int wide, int tall, int radius, bool roundTop, bool roundBottom,
								 int r, int g, int b, int a )
{
	if( wide <= 0 || tall <= 0 )
		return;

	if( radius > wide / 2 ) radius = wide / 2;
	if( radius > tall / 2 ) radius = tall / 2;
	if( radius < 0 ) radius = 0;

	const int rt = roundTop ? radius : 0;
	const int rb = roundBottom ? radius : 0;

	FillRGBABlend( x, y + rt, wide, tall - rt - rb, r, g, b, a );

	for( int i = 0; i < rt; ++i )
	{
		const int inset = RoundedInset( rt, i );
		FillRGBABlend( x + inset, y + i, wide - inset * 2, 1, r, g, b, a );
	}

	for( int i = 0; i < rb; ++i )
	{
		const int inset = RoundedInset( rb, i );
		FillRGBABlend( x + inset, y + tall - 1 - i, wide - inset * 2, 1, r, g, b, a );
	}
}

// Blended rectangle made of 1 pixel rows; the corners on the left and/or on the right side are rounded
void DrawUtils::FillRoundedRectSides( int x, int y, int wide, int tall, int radius, bool roundLeft, bool roundRight,
									  int r, int g, int b, int a )
{
	if( wide <= 0 || tall <= 0 )
		return;

	if( radius > wide / 2 ) radius = wide / 2;
	if( radius > tall / 2 ) radius = tall / 2;
	if( radius < 0 ) radius = 0;

	if( !roundLeft && !roundRight )
		radius = 0;

	FillRGBABlend( x, y + radius, wide, tall - radius * 2, r, g, b, a );

	for( int i = 0; i < radius; ++i )
	{
		const int inset = RoundedInset( radius, i );
		const int left = roundLeft ? inset : 0;
		const int right = roundRight ? inset : 0;

		FillRGBABlend( x + left, y + i, wide - left - right, 1, r, g, b, a );
		FillRGBABlend( x + left, y + tall - 1 - i, wide - left - right, 1, r, g, b, a );
	}
}

// Outline of a rectangle with rounded corners, 1 pixel thick, drawn without blending
void DrawUtils::DrawRoundedFrame( int x, int y, int wide, int tall, int radius, int r, int g, int b, int a )
{
	if( wide <= 1 || tall <= 1 )
		return;

	if( radius > wide / 2 ) radius = wide / 2;
	if( radius > tall / 2 ) radius = tall / 2;
	if( radius < 0 ) radius = 0;

	// straight sides between the corners
	FillRGBA( x + radius, y,            wide - radius * 2, 1, r, g, b, a );
	FillRGBA( x + radius, y + tall - 1, wide - radius * 2, 1, r, g, b, a );
	FillRGBA( x,            y + radius, 1, tall - radius * 2, r, g, b, a );
	FillRGBA( x + wide - 1, y + radius, 1, tall - radius * 2, r, g, b, a );

	// corners: in every row a short piece, so the line has no gaps
	int prevInset = radius;

	for( int i = 0; i < radius; ++i )
	{
		const int inset = RoundedInset( radius, i );
		const int runEnd = ( prevInset - 1 > inset ) ? prevInset - 1 : inset;
		const int len = runEnd - inset + 1;

		FillRGBA( x + inset,                y + i,                len, 1, r, g, b, a );
		FillRGBA( x + wide - 1 - runEnd,    y + i,                len, 1, r, g, b, a );
		FillRGBA( x + inset,                y + tall - 1 - i,     len, 1, r, g, b, a );
		FillRGBA( x + wide - 1 - runEnd,    y + tall - 1 - i,     len, 1, r, g, b, a );

		prevInset = inset;
	}
}

void DrawUtils::DrawRoundedRectangle( int x, int y, int wide, int tall, int radius, int r, int g, int b, int a, bool drawStroke )
{
	FillRoundedRect( x, y, wide, tall, radius, true, true, r, g, b, a );

	if( drawStroke )
		DrawRoundedFrame( x, y, wide, tall, radius, 255, 140, 0, 255 ); // the colour of the stroke of DrawRectangle
}

void DrawUtils::Draw2DQuad( float x1, float y1, float x2, float y2 )
{
	// REMOVE WHEN NANOGL BUG WILL BE FIXED
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	// REMOVE WHEN NANOGL BUG WILL BE FIXED

	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
	gEngfuncs.pTriAPI->Vertex3f( x1, y1, 0 );

	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
	gEngfuncs.pTriAPI->Vertex3f( x1, y2, 0 );

	gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
	gEngfuncs.pTriAPI->Vertex3f( x2, y2, 0 );

	gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
	gEngfuncs.pTriAPI->Vertex3f( x2, y1, 0 );

	// REMOVE WHEN NANOGL BUG WILL BE FIXED
	gEngfuncs.pTriAPI->End( );
	// REMOVE WHEN NANOGL BUG WILL BE FIXED

}

int DrawUtils::HudStringLen( const char *szIt, float scale )
{
	int l;

	Con_UtfProcessChar( 0 );

	// count length until we hit the null character or a newline character
	for ( l = 0; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		if( szIt[0] == '\\' && szIt[1] != '\n' &&
			(szIt[1] == 'y' || szIt[1] == 'w' || szIt[1] == 'd' || szIt[1] == 'R') ) // not sure is reversing handled correctly
		{
			szIt++;
			continue;
		}

		if( szIt[0] == '^' && isdigit( szIt[1] ) ) // suck down, unreadable nicknames. Check even if hud_colored is off
		{
			szIt++;
			continue;
		}

		int uch = Con_UtfProcessChar( (unsigned char)*szIt );

		if ( !uch )
			continue;

		l += gHUD.GetCharWidth( uch );
	}

	return l;
}
