//
// text_texture.cpp
//

#include "hud.h"
#include "cl_util.h"
#include "triangleapi.h"
#include "draw_util.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <vector>

#include "text_texture.h"
#include "glyph_atlas.h"
#include "client_texture.h"

void CTextTexture::Free()
{
	m_pFont = nullptr;
	m_quads.clear();
	m_iWidth = m_iHeight = 0;
	m_bFailed = false;
	m_key.clear();
}

int CTextTexture::Ascent() const
{
	return m_pFont ? m_pFont->Ascent() : 0;
}

bool CTextTexture::Update( const std::string &utf8, const char *fontName, int emHeight, int weight, bool antialias,
						   int maxWidth, bool ellipsis )
{
	if( maxWidth < 8 )
		maxWidth = 8;

	char extra[128];
	snprintf( extra, sizeof( extra ), "|%s|%d|%d|%d|%d|%d", fontName ? fontName : "", emHeight, weight,
		antialias ? 1 : 0, maxWidth, ellipsis ? 1 : 0 );
	const std::string key = utf8 + extra;

	if( key == m_key )
		return !m_bFailed;

	Free();
	m_key = key;

	CGlyphFont *font = CGlyphFont::Get( fontName, emHeight, weight, antialias );
	if( !font )
	{
		m_bFailed = true;
		return false;
	}

	m_pFont = font;

	if( utf8.empty() )
		return true; // nothing to draw

	std::u32string text = GlyphText_Decode( utf8 );
	font->Prepare( text );
	if( ellipsis )
		font->Prepare( U"." );

	auto advanceOf = [font]( char32_t c ) {
		const CGlyphFont::Glyph *g = font->Find( c );
		return g ? g->advance : 0;
	};

	// the lines
	std::vector<std::u32string> lines( 1 );
	for( char32_t c : text )
	{
		if( c == '\n' )
			lines.emplace_back();
		else if( c != '\r' )
			lines.back().push_back( c );
	}

	const int lineH = font->Height();
	const int tab = font->TabWidth();

	for( size_t li = 0; li < lines.size(); ++li )
	{
		std::u32string &line = lines[li];

		// the pen positions of the letters (the tabs go to the next stop, as DT_EXPANDTABS)
		auto measure = [&]( const std::u32string &s ) {
			int pen = 0;
			for( char32_t c : s )
				pen = ( c == '\t' ) ? ( pen / tab + 1 ) * tab : pen + advanceOf( c );
			return pen;
		};

		// too wide with "...": letters are taken away until it fits (as DT_END_ELLIPSIS)
		if( ellipsis && measure( line ) > maxWidth )
		{
			const int dots = advanceOf( '.' ) * 3;
			while( !line.empty() && measure( line ) + dots > maxWidth )
				line.pop_back();
			line += U"...";
		}

		int pen = 0;
		const int top = (int)li * lineH;

		for( char32_t c : line )
		{
			if( c == '\t' )
			{
				pen = ( pen / tab + 1 ) * tab;
				continue;
			}

			// the other control characters (the colour codes of the chat) have no letters
			if( c < 0x20 )
				continue;

			const CGlyphFont::Glyph *g = font->Find( c );
			if( !g )
				g = font->Find( '?' );
			if( !g )
				continue;

			if( g->page >= 0 )
			{
				Quad q;
				q.page = g->page;
				q.x = pen + g->left;
				q.y = top;
				q.w = g->w;
				q.h = g->h;
				q.sx = g->x;
				q.sy = g->y;

				// the picture is cut at the edges of the text (as the old picture of the whole text was)
				if( q.x < 0 )
				{
					q.sx -= q.x;
					q.w += q.x;
					q.x = 0;
				}
				if( q.x + q.w > maxWidth )
					q.w = maxWidth - q.x;

				if( q.w > 0 )
					m_quads.push_back( q );
			}

			pen += g->advance;
		}

		const int w = pen < maxWidth ? pen : maxWidth;
		if( w > m_iWidth )
			m_iWidth = w;
	}

	m_iHeight = (int)lines.size() * lineH;
	return true;
}

void CTextTexture::Draw( float x, float y, int r, int g, int b, int a ) const
{
	if( !m_pFont || m_quads.empty() )
		return;

	// whole pixels: every texel of the atlas lands on its own pixel
	const int ox = (int)floorf( x + 0.5f );
	const int oy = (int)floorf( y + 0.5f );

	gRenderAPI.GL_SelectTexture( 0 );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );

	// the letters of every page together (a text has almost always one page)
	std::vector<int> pages;

	for( const Quad &q : m_quads )
	{
		bool seen = false;
		for( int p : pages )
			seen = seen || p == q.page;
		if( !seen )
			pages.push_back( q.page );
	}

	for( int p : pages )
	{
		const int tex = m_pFont->PageTexture( p );
		if( tex <= 0 )
			continue;

		const float pw = (float)m_pFont->PageWidth( p );
		const float ph = (float)m_pFont->PageHeight( p );

		gRenderAPI.GL_Bind( 0, tex );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );

		for( const Quad &q : m_quads )
		{
			if( q.page != p )
				continue;

			const float x1 = (float)( ox + q.x ), y1 = (float)( oy + q.y );
			const float x2 = x1 + q.w, y2 = y1 + q.h;
			const float s1 = q.sx / pw, t1 = q.sy / ph;
			const float s2 = ( q.sx + q.w ) / pw, t2 = ( q.sy + q.h ) / ph;

			gEngfuncs.pTriAPI->TexCoord2f( s1, t1 );
			gEngfuncs.pTriAPI->Vertex3f( x1, y1, 0 );
			gEngfuncs.pTriAPI->TexCoord2f( s1, t2 );
			gEngfuncs.pTriAPI->Vertex3f( x1, y2, 0 );
			gEngfuncs.pTriAPI->TexCoord2f( s2, t2 );
			gEngfuncs.pTriAPI->Vertex3f( x2, y2, 0 );
			gEngfuncs.pTriAPI->TexCoord2f( s2, t1 );
			gEngfuncs.pTriAPI->Vertex3f( x2, y1, 0 );
		}

		gEngfuncs.pTriAPI->End();
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
}

