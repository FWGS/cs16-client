//
// glyph_atlas.cpp
//

#include "hud.h"
#include "cl_util.h"

#include <stdio.h>
#include <string.h>
#include <memory>

#include "glyph_atlas.h"
#include "glyph_gdi.h"
#include "client_texture.h"

namespace {

std::vector<std::unique_ptr<CGlyphFont>> g_fonts;
int g_pageCounter = 0;

// the letters drawn when a font is made: ASCII, Latin-1, Cyrillic, the usual signs
void InitialChars( std::vector<char32_t> &out )
{
	for( char32_t c = 0x20; c < 0x7F; ++c )
		out.push_back( c );
	for( char32_t c = 0xA0; c <= 0xFF; ++c )
		out.push_back( c );
	for( char32_t c = 0x400; c <= 0x45F; ++c )
		out.push_back( c );
	static const char32_t signs[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x201E, 0x2022, 0x2026,
		0x2116, 0x20AC, 0x2122 };
	for( char32_t c : signs )
		out.push_back( c );
}

} // namespace

std::u32string GlyphText_Decode( const std::string &utf8 )
{
	std::u32string out;
	out.reserve( utf8.size() );

	const unsigned char *p = reinterpret_cast<const unsigned char *>( utf8.c_str() );

	while( *p )
	{
		char32_t c = *p;
		int extra = 0;

		if( c >= 0xF0 && c <= 0xF4 ) { c &= 0x07; extra = 3; }
		else if( c >= 0xE0 ) { c &= 0x0F; extra = 2; }
		else if( c >= 0xC2 && c < 0xE0 ) { c &= 0x1F; extra = 1; }
		else if( c >= 0x80 ) { out.push_back( c ); ++p; continue; } // not UTF-8: as a byte of Windows-1252

		int i = 1;
		for( ; i <= extra; ++i )
		{
			if(( p[i] & 0xC0 ) != 0x80 )
				break;
			c = ( c << 6 ) | ( p[i] & 0x3F );
		}

		if( i <= extra )
		{
			out.push_back( *p ); // a broken sequence: the first byte alone
			++p;
			continue;
		}

		out.push_back( c );
		p += extra + 1;
	}

	return out;
}

CGlyphFont *CGlyphFont::Get( const char *face, int emHeight, int weight, bool antialias )
{
	if( emHeight < 4 )
		emHeight = 4;

	char key[128];
	snprintf( key, sizeof( key ), "%s|%d|%d|%d", face ? face : "", emHeight, weight, antialias ? 1 : 0 );

	for( const auto &f : g_fonts )
	{
		if( f->m_key == key )
			return f->m_font ? f.get() : nullptr;
	}

	std::unique_ptr<CGlyphFont> font( new CGlyphFont );
	font->m_key = key;

	GlyphFontInfo info;
	font->m_font = GlyphGDI_CreateFont( face, emHeight, weight, antialias, info );

	if( font->m_font )
	{
		font->m_iHeight = info.height;
		font->m_iAscent = info.ascent;
		font->m_iAveCharWidth = info.aveCharWidth;

		std::vector<char32_t> chars;
		InitialChars( chars );
		font->AddGlyphs( chars );
	}

	CGlyphFont *result = font->m_font ? font.get() : nullptr;
	g_fonts.push_back( std::move( font )); // also remembers that it did not work
	return result;
}

void CGlyphFont::Prepare( const std::u32string &text )
{
	std::vector<char32_t> chars;

	for( char32_t c : text )
	{
		if( c < 0x20 )
			continue; // line ends, tabs, the colour codes of the chat: no letters
		if( m_glyphs.count( c ) || m_missing.count( c ))
			continue;

		bool seen = false;
		for( char32_t d : chars )
			seen = seen || d == c;
		if( !seen )
			chars.push_back( c );
	}

	if( !chars.empty() )
		AddGlyphs( chars );
}

const CGlyphFont::Glyph *CGlyphFont::Find( char32_t ch ) const
{
	auto it = m_glyphs.find( ch );
	return it != m_glyphs.end() ? &it->second : nullptr;
}

// The letters are drawn and packed into a new page in rows of the height of the font, a pixel apart
void CGlyphFont::AddGlyphs( const std::vector<char32_t> &chars )
{
	if( !m_font || chars.empty() )
		return;

	struct Item { char32_t ch; GlyphImage img; };
	std::vector<Item> items;
	items.reserve( chars.size() );

	for( char32_t c : chars )
	{
		Item it;
		it.ch = c;
		if( !GlyphGDI_Render( m_font, (unsigned int)c, it.img ))
		{
			m_missing[c] = true;
			continue;
		}
		items.push_back( std::move( it ));
	}

	if( items.empty() )
		return;

	const int pageW = m_iHeight >= 40 ? 1024 : 512;
	const int maxPageH = 2048;
	const int rowH = m_iHeight + 1;

	size_t first = 0;

	while( first < items.size() )
	{
		// how many letters fit into one page
		int x = 0, y = 0;
		size_t last = first;

		for( ; last < items.size(); ++last )
		{
			const GlyphImage &img = items[last].img;
			int w = img.w > 0 ? img.w : 0;
			if( w > pageW )
				w = pageW;

			if( x + w > pageW )
			{
				x = 0;
				y += rowH;
			}
			if( y + rowH > maxPageH )
				break;
			x += w + 1;
		}

		if( last == first )
			break; // a letter higher than a page: not possible with the sizes of the HUD

		int pageH = 16;
		while( pageH < y + rowH )
			pageH *= 2;

		Page page;
		page.w = pageW;
		page.h = pageH;
		page.alpha.assign( (size_t)pageW * pageH, 0 );

		const int pageIndex = (int)m_pages.size();
		x = 0;
		y = 0;

		for( size_t i = first; i < last; ++i )
		{
			const GlyphImage &img = items[i].img;
			int w = img.w > 0 ? img.w : 0;
			if( w > pageW )
				w = pageW;

			if( x + w > pageW )
			{
				x = 0;
				y += rowH;
			}

			Glyph g;
			g.left = img.left;
			g.advance = img.advance;

			bool empty = true;
			for( unsigned char a : img.alpha )
				empty = empty && a == 0;

			if( !empty && w > 0 )
			{
				g.page = pageIndex;
				g.x = x;
				g.y = y;
				g.w = w;
				g.h = img.h < m_iHeight ? img.h : m_iHeight;

				for( int row = 0; row < g.h; ++row )
					memcpy( &page.alpha[(size_t)( y + row ) * pageW + x], &img.alpha[(size_t)row * img.w], (size_t)w );

				x += w + 1;
			}

			m_glyphs[items[i].ch] = g;
		}

		m_pages.push_back( std::move( page ));
		UploadPage( m_pages.back() );
		first = last;
	}
}

bool CGlyphFont::UploadPage( Page &page )
{
	if( !gRenderAPI.GL_CreateTexture )
		return false;

	// white pixels, the letters are in the alpha channel (the colour is set when the text is drawn)
	std::vector<unsigned char> rgba( (size_t)page.w * page.h * 4, 255 );
	for( size_t i = 0, n = (size_t)page.w * page.h; i < n; ++i )
		rgba[i * 4 + 3] = page.alpha[i];

	// every page has its own name: the engine gives back the old texture for a name it already has
	char name[48];
	snprintf( name, sizeof( name ), "%s%d", TexturePrefix(), ++g_pageCounter );

	const int tex = gRenderAPI.GL_CreateTexture( name, page.w, page.h, rgba.data(),
		static_cast<texFlags_t>( TF_NOMIPMAP | TF_CLAMP | TF_HAS_ALPHA | TF_NEAREST ));

	if( tex <= 0 )
	{
		page.tex = 0;
		page.name.clear();
		return false;
	}

	page.tex = tex;
	page.name = name;
	return true;
}

int CGlyphFont::PageTexture( int index )
{
	if( index < 0 || index >= (int)m_pages.size() )
		return 0;

	Page &page = m_pages[index];

	if( page.tex > 0 && ClientTexture::Owned( page.tex, page.name.c_str() ))
		return page.tex;

	// the engine has freed it (or it was never made): made again from the pixels kept
	if( page.tex > 0 )
		ClientTexture::ReportLost( page.name.c_str() );

	static float lastTry = -1000.0f;
	const float now = gEngfuncs.GetClientTime();
	if( page.tex <= 0 && page.name.empty() && now - lastTry < 1.0f && now >= lastTry )
		return 0; // it did not work a moment ago: not every frame

	lastTry = now;
	return UploadPage( page ) ? page.tex : 0;
}
