//
// glyph_atlas.h
//
// The fonts of Windows as atlases: the letters of a font are drawn once (glyph_gdi.cpp) into one texture and the
// texts are put together from them, as the VGUI2 fonts of the Steam version do. A new text costs nothing (no new
// texture), only letters that are not there yet are added (a new page of the atlas, rarely: the Latin, Latin-1
// and Cyrillic letters and the usual signs are drawn at once).
//
// No dependencies on the engine besides the textures (gRenderAPI) and the triangles (pTriAPI) of the drawing.
//

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

class CGlyphFont
{
public:
	struct Glyph
	{
		int page = -1;           // -1: an empty letter (a space), nothing to draw
		int x = 0, y = 0, w = 0, h = 0; // the picture in its page
		int left = 0;            // where the picture starts from the pen
		int advance = 0;         // how far the pen moves
	};

	// the font (made once and kept); nullptr if there are no fonts of Windows (the other systems)
	static CGlyphFont *Get( const char *face, int emHeight, int weight, bool antialias );

	int Height() const { return m_iHeight; }
	int Ascent() const { return m_iAscent; }
	int TabWidth() const { return m_iAveCharWidth * 8 > 0 ? m_iAveCharWidth * 8 : 8; } // as DrawText

	// adds the letters of the text that are not in the atlas yet (one new page for all of them)
	void Prepare( const std::u32string &text );

	// a letter of the atlas (Prepare has been called for it); nullptr if the font has no such letter
	const Glyph *Find( char32_t ch ) const;

	// the texture of a page, made again if the engine has freed it; 0 if it cannot be made
	int PageTexture( int page );
	int PageWidth( int page ) const { return page >= 0 && page < (int)m_pages.size() ? m_pages[page].w : 1; }
	int PageHeight( int page ) const { return page >= 0 && page < (int)m_pages.size() ? m_pages[page].h : 1; }

	// the texture names of the atlases start with this (cl_textures_check counts them)
	static const char *TexturePrefix() { return "*glyphs_"; }

private:
	struct Page
	{
		int w = 0, h = 0;
		std::vector<unsigned char> alpha; // kept: the texture is made again from it if the engine frees it
		int tex = 0;
		std::string name;
	};

	CGlyphFont() = default;
	void AddGlyphs( const std::vector<char32_t> &chars );
	bool UploadPage( Page &page );

	std::string m_key;
	void *m_font = nullptr; // glyph_gdi.h
	int m_iHeight = 0, m_iAscent = 0, m_iAveCharWidth = 0;
	std::unordered_map<char32_t, Glyph> m_glyphs;
	std::unordered_map<char32_t, bool> m_missing; // the letters the font could not draw
	std::vector<Page> m_pages;
};

// UTF-8 to code points (a wrong byte is taken as a Windows-1252 character)
std::u32string GlyphText_Decode( const std::string &utf8 );
