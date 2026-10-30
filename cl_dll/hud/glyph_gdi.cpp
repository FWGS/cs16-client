//
// glyph_gdi.cpp
//
// The letters of a Windows font for the atlas (see glyph_gdi.h)
//

#include <string.h>
#include <string>
#include <vector>

#include "glyph_gdi.h"

#if defined( _WIN32 )

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wchar.h>

namespace {

// the light pixels of the antialiasing outside the ABC box of a letter are kept: its picture is this much wider
// on both sides
const int GLYPH_PAD = 2;

struct GdiFont
{
	HFONT font = nullptr;
	int height = 0;
	int maxWidth = 0;
};

HDC SharedDC( void )
{
	// one memory DC for everything (GetDC of the screen is slow while the game is shown: it waits for the desktop)
	static HDC dc = nullptr;
	if( !dc )
		dc = CreateCompatibleDC( nullptr );
	return dc;
}

std::wstring Utf8ToWide( const char *utf8 )
{
	const int n = MultiByteToWideChar( CP_UTF8, 0, utf8, -1, nullptr, 0 );

	if( n <= 1 )
		return std::wstring();

	std::wstring w( (size_t)n, L'\0' );
	MultiByteToWideChar( CP_UTF8, 0, utf8, -1, &w[0], n );
	w.resize( (size_t)n - 1 );
	return w;
}

// The font asked for, if the system has it; otherwise the next one of the list that it has
HFONT CreateFontFromList( HDC dc, const char *wanted, int emHeight, int weight, bool antialias )
{
	const std::wstring first = Utf8ToWide( wanted ? wanted : "" );
	const wchar_t *fallbacks[] = { L"Tahoma", L"Verdana", L"Segoe UI", L"Arial" };
	const DWORD quality = antialias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY;

	for( int i = -1; i < (int)( sizeof( fallbacks ) / sizeof( fallbacks[0] )); ++i )
	{
		const wchar_t *face = i < 0 ? first.c_str() : fallbacks[i];

		if( !face[0] || wcslen( face ) >= LF_FACESIZE )
			continue;

		// a negative height is the height of the letters (em)
		HFONT font = CreateFontW( -emHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, DEFAULT_PITCH | FF_SWISS, face );

		if( !font )
			continue;

		HGDIOBJ old = SelectObject( dc, font );
		wchar_t actual[LF_FACESIZE] = {};
		GetTextFaceW( dc, LF_FACESIZE, actual );
		SelectObject( dc, old );

		if( _wcsicmp( actual, face ) == 0 )
			return font;

		DeleteObject( font ); // the system gave another font instead
	}

	return CreateFontW( -emHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, DEFAULT_PITCH | FF_SWISS, L"Arial" );
}

} // namespace

void *GlyphGDI_CreateFont( const char *face, int emHeight, int weight, bool antialias, GlyphFontInfo &info )
{
	HDC dc = SharedDC();
	if( !dc || emHeight < 4 )
		return nullptr;

	HFONT font = CreateFontFromList( dc, face, emHeight, weight, antialias );
	if( !font )
		return nullptr;

	HGDIOBJ old = SelectObject( dc, font );
	TEXTMETRICW tm = {};
	const bool ok = GetTextMetricsW( dc, &tm ) != 0;
	SelectObject( dc, old );

	if( !ok || tm.tmHeight <= 0 )
	{
		DeleteObject( font );
		return nullptr;
	}

	GdiFont *f = new GdiFont;
	f->font = font;
	f->height = tm.tmHeight;
	f->maxWidth = tm.tmMaxCharWidth + tm.tmOverhang;

	info.height = tm.tmHeight;
	info.ascent = tm.tmAscent;
	info.aveCharWidth = tm.tmAveCharWidth > 0 ? tm.tmAveCharWidth : emHeight / 2;
	return f;
}

bool GlyphGDI_Render( void *handle, unsigned int ch, GlyphImage &out )
{
	GdiFont *f = static_cast<GdiFont *>( handle );
	HDC dc = SharedDC();

	if( !f || !dc || ch > 0xFFFF )
		return false;

	HGDIOBJ oldFont = SelectObject( dc, f->font );

	// the metrics GDI moves the pen by in a string
	ABC abc = {};
	if( !GetCharABCWidthsW( dc, ch, ch, &abc ))
	{
		INT width = 0;
		if( !GetCharWidth32W( dc, ch, ch, &width ))
		{
			SelectObject( dc, oldFont );
			return false;
		}
		abc.abcA = 0;
		abc.abcB = (UINT)width;
		abc.abcC = 0;
	}

	out.advance = abc.abcA + (int)abc.abcB + abc.abcC;
	out.left = abc.abcA - GLYPH_PAD;
	out.w = (int)abc.abcB + GLYPH_PAD * 2;
	out.h = f->height;
	out.alpha.assign( (size_t)( out.w > 0 ? out.w : 0 ) * out.h, 0 );

	bool ok = true;

	if( out.w > 0 && out.h > 0 && ch != ' ' && ch != '\t' )
	{
		BITMAPINFO bi = {};
		bi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
		bi.bmiHeader.biWidth = out.w;
		bi.bmiHeader.biHeight = -out.h; // from the top to the bottom
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;

		void *bits = nullptr;
		HBITMAP bitmap = CreateDIBSection( dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0 );

		if( bitmap && bits )
		{
			HGDIOBJ oldBitmap = SelectObject( dc, bitmap );

			memset( bits, 0, (size_t)out.w * out.h * 4 );
			SetTextAlign( dc, TA_LEFT | TA_TOP | TA_NOUPDATECP );
			SetBkColor( dc, RGB( 0, 0, 0 ));
			SetTextColor( dc, RGB( 255, 255, 255 ));
			SetBkMode( dc, TRANSPARENT );

			// the pen GLYPH_PAD pixels before "A": the letter lands where it would be in a string
			const wchar_t wch = (wchar_t)ch;
			ExtTextOutW( dc, GLYPH_PAD - abc.abcA, 0, 0, nullptr, &wch, 1, nullptr );
			GdiFlush();

			// white text on black: the brightness of a pixel is how much of it the letter covers
			const unsigned char *p = static_cast<const unsigned char *>( bits );

			for( size_t i = 0, n = (size_t)out.w * out.h; i < n; ++i, p += 4 )
			{
				unsigned char m = p[0] > p[1] ? p[0] : p[1];
				out.alpha[i] = m > p[2] ? m : p[2];
			}

			SelectObject( dc, oldBitmap );
		}
		else
		{
			ok = false;
		}

		if( bitmap )
			DeleteObject( bitmap );
	}

	SelectObject( dc, oldFont );
	return ok;
}

void GlyphGDI_FreeFont( void *handle )
{
	GdiFont *f = static_cast<GdiFont *>( handle );
	if( !f )
		return;

	if( f->font )
		DeleteObject( f->font );
	delete f;
}

#elif !defined( CS16CLIENT_FREETYPE ) // no fonts of Windows, no FreeType: the font of the HUD is used (see glyph_freetype.cpp)

void *GlyphGDI_CreateFont( const char *, int, int, bool, GlyphFontInfo & )
{
	return nullptr;
}

bool GlyphGDI_Render( void *, unsigned int, GlyphImage & )
{
	return false;
}

void GlyphGDI_FreeFont( void * )
{
}

#endif
