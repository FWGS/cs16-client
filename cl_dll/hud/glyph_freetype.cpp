//
// glyph_freetype.cpp
//
// The letters for the atlases of glyph_atlas.h on the systems without the fonts of Windows (Linux): FreeType draws
// them from the font files that fontconfig finds. The font asked for is used if the system has it (Verdana and
// Tahoma come with the "Microsoft core fonts" packages: ttf-mscorefonts-installer, msttcore-fonts), else the font
// fontconfig puts in its place (DejaVu Sans, Liberation Sans...). A letter the font has not is taken from another
// font that has it. The metrics are those GDI gives on Windows: the em of the font is the asked height, the line is
// ascent + descent, the pen moves by the advance of the letter.
//
// Built when CMake finds freetype2 and fontconfig (CS16CLIENT_FREETYPE); without them glyph_gdi.cpp returns no
// font and the client uses the font of the HUD.
//

#if !defined( _WIN32 ) && defined( CS16CLIENT_FREETYPE )

#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include <fontconfig/fontconfig.h>

#include "glyph_gdi.h"

namespace {

FT_Library g_library = nullptr;
bool g_initFailed = false;

bool InitLibraries( void )
{
	if( g_library )
		return true;
	if( g_initFailed )
		return false;

	if( FT_Init_FreeType( &g_library ) != 0 || !FcInit() )
	{
		g_library = nullptr;
		g_initFailed = true;
		return false;
	}
	return true;
}

// the file (and the index of the face in it) that fontconfig gives for a family, a weight and, if given, a letter
// the font must have
bool FindFontFile( const char *family, int weight, unsigned int ch, std::string &path, int &index, bool &bold )
{
	FcPattern *pattern = FcPatternCreate();
	if( !pattern )
		return false;

	FcPatternAddString( pattern, FC_FAMILY, (const FcChar8 *)( family && family[0] ? family : "Verdana" ));
	// without the font asked for: DejaVu Sans (made from Bitstream Vera, drawn by the author of Verdana: the closest
	// widths and shapes among the free fonts), then what the system has
	FcPatternAddString( pattern, FC_FAMILY, (const FcChar8 *)"DejaVu Sans" );
	FcPatternAddString( pattern, FC_FAMILY, (const FcChar8 *)"sans-serif" );
	FcPatternAddInteger( pattern, FC_WEIGHT, weight >= 600 ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR );
	FcPatternAddBool( pattern, FC_SCALABLE, FcTrue );

	FcCharSet *charset = nullptr;
	if( ch )
	{
		charset = FcCharSetCreate();
		FcCharSetAddChar( charset, ch );
		FcPatternAddCharSet( pattern, FC_CHARSET, charset );
	}

	FcConfigSubstitute( nullptr, pattern, FcMatchPattern );
	FcDefaultSubstitute( pattern );

	FcResult result = FcResultNoMatch;
	FcPattern *match = FcFontMatch( nullptr, pattern, &result );
	bool ok = false;

	if( match )
	{
		FcChar8 *file = nullptr;
		int faceIndex = 0, matchWeight = FC_WEIGHT_REGULAR;
		FcCharSet *has = nullptr;

		if( FcPatternGetString( match, FC_FILE, 0, &file ) == FcResultMatch && file )
		{
			// a font that has the letter, when it was asked for
			ok = !ch || ( FcPatternGetCharSet( match, FC_CHARSET, 0, &has ) == FcResultMatch && FcCharSetHasChar( has, ch ));
			if( ok )
			{
				path = (const char *)file;
				FcPatternGetInteger( match, FC_INDEX, 0, &faceIndex );
				FcPatternGetInteger( match, FC_WEIGHT, 0, &matchWeight );
				index = faceIndex;
				bold = matchWeight >= FC_WEIGHT_DEMIBOLD;
			}
		}
		FcPatternDestroy( match );
	}

	if( charset )
		FcCharSetDestroy( charset );
	FcPatternDestroy( pattern );
	return ok;
}

struct FtFace
{
	FT_Face face = nullptr;
	bool embolden = false; // a bold font was asked for, the file is not bold: the letters are made bold
};

struct FtFont
{
	std::string family;
	int em = 0, weight = 0;
	bool antialias = true;
	int height = 0, ascent = 0;
	FtFace main;
	std::unordered_map<std::string, FtFace> fallbacks; // other files, for the letters the main one has not
};

bool OpenFace( const std::string &path, int index, int em, bool wantBold, bool isBold, FtFace &out )
{
	FT_Face face = nullptr;
	if( FT_New_Face( g_library, path.c_str(), index, &face ) != 0 )
		return false;

	if( FT_Set_Pixel_Sizes( face, 0, (FT_UInt)em ) != 0 )
	{
		FT_Done_Face( face );
		return false;
	}

	out.face = face;
	out.embolden = wantBold && !isBold;
	return true;
}

// the face that draws the letter: the main one, or another file that has it
FtFace *FaceFor( FtFont *f, unsigned int ch )
{
	if( FT_Get_Char_Index( f->main.face, ch ))
		return &f->main;

	std::string path;
	int index = 0;
	bool bold = false;
	if( !FindFontFile( f->family.c_str(), f->weight, ch, path, index, bold ))
		return nullptr;

	const std::string key = path + "#" + std::to_string( index );
	auto it = f->fallbacks.find( key );
	if( it == f->fallbacks.end() )
	{
		FtFace face;
		if( !OpenFace( path, index, f->em, f->weight >= 600, bold, face ))
			return nullptr;
		it = f->fallbacks.emplace( key, face ).first;
	}

	return FT_Get_Char_Index( it->second.face, ch ) ? &it->second : nullptr;
}

} // namespace

void *GlyphGDI_CreateFont( const char *face, int emHeight, int weight, bool antialias, GlyphFontInfo &info )
{
	if( emHeight < 4 || !InitLibraries() )
		return nullptr;

	std::string path;
	int index = 0;
	bool bold = false;
	if( !FindFontFile( face, weight, 0, path, index, bold ))
		return nullptr;

	FtFont *f = new FtFont;
	f->family = face ? face : "";
	f->em = emHeight;
	f->weight = weight;
	f->antialias = antialias;

	if( !OpenFace( path, index, emHeight, weight >= 600, bold, f->main ))
	{
		delete f;
		return nullptr;
	}

	// the line as GDI makes it: ascent + descent of the font, in whole pixels
	const FT_Size_Metrics &m = f->main.face->size->metrics;
	f->ascent = (int)(( m.ascender + 63 ) >> 6 );
	const int descent = (int)(( -m.descender + 63 ) >> 6 );
	f->height = f->ascent + descent;
	if( f->height <= 0 )
	{
		f->ascent = emHeight;
		f->height = emHeight + emHeight / 4;
	}

	info.height = f->height;
	info.ascent = f->ascent;
	info.aveCharWidth = emHeight / 2;

	// the average width of GDI is about the width of "x"
	if( FT_Load_Char( f->main.face, 'x', FT_LOAD_DEFAULT ) == 0 )
		info.aveCharWidth = (int)(( f->main.face->glyph->advance.x + 32 ) >> 6 );

	return f;
}

bool GlyphGDI_Render( void *handle, unsigned int ch, GlyphImage &out )
{
	FtFont *f = static_cast<FtFont *>( handle );
	if( !f || !f->main.face )
		return false;

	FtFace *face = FaceFor( f, ch );
	if( !face )
		return false;

	const FT_Int32 target = f->antialias ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO;
	if( FT_Load_Char( face->face, ch, FT_LOAD_DEFAULT | target ) != 0 )
		return false;

	FT_GlyphSlot slot = face->face->glyph;
	if( face->embolden )
		FT_GlyphSlot_Embolden( slot );

	if( FT_Render_Glyph( slot, f->antialias ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO ) != 0 )
		return false;

	const FT_Bitmap &bm = slot->bitmap;
	out.advance = (int)(( slot->advance.x + 32 ) >> 6 );
	out.left = slot->bitmap_left;
	out.w = (int)bm.width;
	out.h = f->height;
	out.alpha.assign( (size_t)( out.w > 0 ? out.w : 0 ) * out.h, 0 );

	// the picture in the cell of the line: its top is "bitmap_top" above the baseline
	const int top = f->ascent - slot->bitmap_top;

	for( int row = 0; row < (int)bm.rows; ++row )
	{
		const int y = top + row;
		if( y < 0 || y >= out.h )
			continue;

		const unsigned char *src = bm.buffer + row * bm.pitch;
		for( int x = 0; x < out.w; ++x )
		{
			unsigned char a;
			if( bm.pixel_mode == FT_PIXEL_MODE_MONO )
				a = ( src[x >> 3] & ( 0x80 >> ( x & 7 ))) ? 255 : 0;
			else
				a = src[x];
			out.alpha[(size_t)y * out.w + x] = a;
		}
	}

	return true;
}

void GlyphGDI_FreeFont( void *handle )
{
	FtFont *f = static_cast<FtFont *>( handle );
	if( !f )
		return;

	if( f->main.face )
		FT_Done_Face( f->main.face );
	for( auto &it : f->fallbacks )
		if( it.second.face )
			FT_Done_Face( it.second.face );
	delete f;
}

#endif
