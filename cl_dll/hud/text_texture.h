//
// text_texture.h
//
// A piece of text in a font of Windows, put together from the atlas of the font (glyph_atlas.h): a new text costs
// no new texture. Used where the client draws text like the VGUI2 panels of the Steam version.
//

#pragma once

#include <string>
#include <vector>

class CGlyphFont;

class CTextTexture
{
public:
	// Lays the text out if something has changed. Returns false if the fonts of Windows are not available
	// (then the caller draws the text with the font of the HUD). Line ends start new lines; a line wider than
	// maxWidth pixels is cut there, or ends with "..." if ellipsis is set.
	bool Update( const std::string &utf8, const char *fontName, int emHeight, int weight, bool antialias,
				 int maxWidth = 4096, bool ellipsis = false );

	// Draws the text with the top left corner of its cell at (x, y), in pixels of the screen
	void Draw( float x, float y, int r, int g, int b, int a = 255 ) const;

	void Free();

	int Width() const { return m_iWidth; }   // the widest line (the advance of its letters)
	int Height() const { return m_iHeight; } // the lines (the height of the font each)
	int Ascent() const;                      // from the top of a line to its baseline
	bool Valid() const { return m_pFont != nullptr; }

private:
	struct Quad
	{
		int page;
		int x, y, w, h;   // on the screen, from the top left corner of the text
		int sx, sy;       // in the page
	};

	CGlyphFont *m_pFont = nullptr;
	std::vector<Quad> m_quads;
	int m_iWidth = 0, m_iHeight = 0;
	bool m_bFailed = false;
	std::string m_key;
};
