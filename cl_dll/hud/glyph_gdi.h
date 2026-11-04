//
// glyph_gdi.h
//
// The letters of a Windows font, one at a time (glyph_gdi.cpp), for the atlas of glyph_atlas.h. Every letter is
// drawn by ExtTextOut as Windows draws it in a string (the same antialiasing) and comes with the metrics GDI moves
// the pen by (ABC widths), so a text put together from the atlas looks like the same text drawn by DrawText.
// Kept apart because <windows.h> conflicts with the headers of the engine. On the other systems the same functions
// are made with FreeType and fontconfig (glyph_freetype.cpp); without them GlyphGDI_CreateFont returns nullptr and
// the client uses the font of the HUD.
//

#pragma once

#include <vector>

struct GlyphFontInfo
{
	int height = 0;       // the height of a line (the cell of the font: ascent + descent)
	int ascent = 0;       // from the top of the cell to the baseline
	int aveCharWidth = 0; // for the tab stops (every 8 average characters, as DrawText)
};

struct GlyphImage
{
	int left = 0;    // where the picture starts from the pen (may be negative)
	int advance = 0; // how far the pen moves
	int w = 0, h = 0;
	std::vector<unsigned char> alpha; // w * h: how much of every pixel the letter covers
};

// a font of Windows: the one asked for if the system has it, else Tahoma, Verdana, Segoe UI, Arial;
// emHeight - the height of the letters (em) in pixels; nullptr if there are no fonts of Windows
void *GlyphGDI_CreateFont( const char *face, int emHeight, int weight, bool antialias, GlyphFontInfo &info );

// one letter (a UTF-16 code unit: the letters beyond U+FFFF are not drawn)
bool GlyphGDI_Render( void *font, unsigned int ch, GlyphImage &out );

void GlyphGDI_FreeFont( void *font );
