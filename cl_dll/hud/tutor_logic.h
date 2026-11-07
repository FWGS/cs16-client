//
// tutor_logic.h
//
// Tutor messages of the server (TutorText / TutorClose / TutorState / TutorLine, as sent by ReGameDLL
// and by AMX Mod X plugins): the colours and the font from resource/TutorScheme.res, the text with its
// parameters and the layout of the window.
// No dependencies on the engine, so it can be tested alone.
//
// The window is the one of the Steam version, measured on its screenshots (1920x1080); the sizes are
// the ones of resource/UI/TutorTextWindow.res and do not depend on the resolution:
//  - in the upper right corner, 10 pixels from the right and from the top
//    (10 pixels below the upper bar of the spectator mode when it is shown);
//  - a dark rounded panel (radius 10), 45 pixels high for one line of text;
//  - on the left a part 40 pixels wide with the icon 25x25 at (10, 10), then a cut of 2 pixels
//    through which the game is seen, then the text from x = 50 with 17 pixels on the right;
//  - the text is in one line (the Steam version does not wrap it, the window just gets wider),
//    Tahoma 14 of the scheme drawn 10 pixels high (em), not antialiased, in the colour of the type.
//

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace Tutor {

// Types of messages (ReGameDLL, tutor_cs_tutor.h): the server sends one of these bits
enum
{
	TYPE_DEFAULT      = 1 << 0, // info, green
	TYPE_FRIEND_DEATH = 1 << 1, // skull, red
	TYPE_ENEMY_DEATH  = 1 << 2, // skull, blue
	TYPE_SCENARIO     = 1 << 3, // info, yellow
	TYPE_BUY          = 1 << 4,
	TYPE_CAREER       = 1 << 5,
	TYPE_HINT         = 1 << 6,
	TYPE_INGAME_HINT  = 1 << 7,
	TYPE_END_GAME     = 1 << 8, // info, yellow
};

enum Icon
{
	ICON_INFO = 0,
	ICON_SKULL,
	ICON_COUNT
};

struct Color
{
	unsigned char r, g, b, a;
};

inline bool SameColor( const Color& x, const Color& y )
{
	return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a;
}

// What the window looks like (defaults are the values of TutorScheme.res of the Steam version)
struct Scheme
{
	Color background = { 0, 0, 0, 200 };       // "WindowBG"
	Color classDefault = { 28, 207, 61, 255 };   // "TutorTextClassDefault"

	struct ClassColor
	{
		int type;
		Color color;
	};
	std::vector<ClassColor> classes = {
		{ TYPE_FRIEND_DEATH, { 255, 38, 38, 255 } },  // "TutorTextClass2"
		{ TYPE_ENEMY_DEATH,  { 38, 149, 253, 255 } }, // "TutorTextClass4"
		{ TYPE_SCENARIO,     { 255, 227, 38, 255 } }, // "TutorTextClass8"
		{ TYPE_END_GAME,     { 255, 227, 38, 255 } }, // "TutorTextClass256"
	};

	std::string fontName = "Tahoma"; // Fonts / "Default" / "1"
	int fontTall = 14;
	int fontWeight = 500;
	bool fontAntialias = false;

	void SetClassColor( int type, const Color& c )
	{
		for( ClassColor& cc : classes )
		{
			if( cc.type == type )
			{
				cc.color = c;
				return;
			}
		}

		classes.push_back( { type, c } );
	}

	Color ColorForType( int type ) const
	{
		for( const ClassColor& cc : classes )
			if( cc.type == type )
				return cc.color;

		return classDefault;
	}
};

inline Icon IconForType( int type )
{
	return ( type == TYPE_FRIEND_DEATH || type == TYPE_ENEMY_DEATH ) ? ICON_SKULL : ICON_INFO;
}

// ---- reading of resource/TutorScheme.res (the KeyValues format of the Valve resource files)

namespace detail {

inline bool IsSpace( char c )
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Next token: a quoted string, '{', '}' or a bare word. Comments (//) are skipped. Returns false at the end.
inline bool NextToken( const char*& p, std::string& token, bool& quoted )
{
	for( ;; )
	{
		while( *p && IsSpace( *p ))
			p++;

		if( p[0] == '/' && p[1] == '/' )
		{
			while( *p && *p != '\n' )
				p++;
			continue;
		}

		break;
	}

	if( !*p )
		return false;

	token.clear();
	quoted = false;

	if( *p == '{' || *p == '}' )
	{
		token = *p++;
		return true;
	}

	if( *p == '"' )
	{
		quoted = true;
		p++;

		while( *p && *p != '"' && token.size() < 1024 )
			token += *p++;

		if( *p == '"' )
			p++;

		return true;
	}

	while( *p && !IsSpace( *p ) && *p != '{' && *p != '}' && *p != '"' && token.size() < 1024 )
		token += *p++;

	return true;
}

inline std::string Lower( std::string s )
{
	for( char& c : s )
		if( c >= 'A' && c <= 'Z' )
			c = (char)( c - 'A' + 'a' );
	return s;
}

inline bool ParseColor( const std::string& s, Color& out )
{
	int v[4] = { 0, 0, 0, 255 };
	int n = 0;
	const char* p = s.c_str();

	while( n < 4 )
	{
		char *end = nullptr;
		const long x = strtol( p, &end, 10 );

		if( end == p )
			break;

		v[n++] = x < -1000 ? -1000 : ( x > 1000 ? 1000 : (int)x );
		p = end;
	}

	if( n < 3 )
		return false;

	for( int& x : v )
		x = x < 0 ? 0 : ( x > 255 ? 255 : x );

	out = { (unsigned char)v[0], (unsigned char)v[1], (unsigned char)v[2], (unsigned char)v[3] };
	return true;
}

} // namespace detail

// Takes from the text of TutorScheme.res what the window uses: the colours of the background and of the
// types of the messages, and the first font of "Default". Everything else is kept as it is in the scheme.
// Values that are missing or wrong keep their defaults.
inline void ParseScheme( const char* text, Scheme& scheme )
{
	if( !text )
		return;

	using namespace detail;

	std::vector<std::string> path; // the sections we are in, in lower case
	std::string token, key;
	bool quoted = false;
	bool haveKey = false;
	bool fontTaken = false;
	const char* p = text;

	while( NextToken( p, token, quoted ))
	{
		if( !quoted && token == "{" )
		{
			path.push_back( haveKey ? Lower( key ) : std::string());
			haveKey = false;
			continue;
		}

		if( !quoted && token == "}" )
		{
			if( !path.empty() )
			{
				// the end of the first font of "Default": the next ones are only fallbacks
				if( path.size() >= 4 && path[1] == "fonts" && path[2] == "default" )
					fontTaken = true;
				path.pop_back();
			}
			haveKey = false;
			continue;
		}

		if( !haveKey )
		{
			key = token;
			haveKey = true;
			continue;
		}

		// key + value
		const std::string lk = Lower( key );
		haveKey = false;

		if( path.size() == 2 && path[1] == "colors" )
		{
			Color c;

			if( !ParseColor( token, c ))
				continue;

			if( lk == "windowbg" )
				scheme.background = c;
			else if( lk == "tutortextclassdefault" )
				scheme.classDefault = c;
			else if( lk.compare( 0, 14, "tutortextclass" ) == 0 && lk.size() > 14 && lk[14] >= '0' && lk[14] <= '9' )
				scheme.SetClassColor( atoi( lk.c_str() + 14 ), c );
		}
		else if( path.size() == 4 && path[1] == "fonts" && path[2] == "default" && !fontTaken )
		{
			if( lk == "name" && !token.empty() && token.size() < 32 )
				scheme.fontName = token;
			else if( lk == "tall" )
			{
				const int t = atoi( token.c_str() );
				if( t >= 6 && t <= 48 )
					scheme.fontTall = t;
			}
			else if( lk == "weight" )
			{
				const int w = atoi( token.c_str() );
				if( w >= 100 && w <= 900 )
					scheme.fontWeight = w;
			}
			else if( lk == "antialias" )
				scheme.fontAntialias = atoi( token.c_str() ) != 0;
		}
	}
}

// ---- the text

// Puts the parameters of the message into its text: %s1 .. %s9 (as the Steam version does)
inline std::string Substitute( const std::string& text, const std::vector<std::string>& args )
{
	std::string out;
	out.reserve( text.size() );

	for( size_t i = 0; i < text.size(); )
	{
		if( text[i] == '%' && i + 2 < text.size() && text[i + 1] == 's' && text[i + 2] >= '1' && text[i + 2] <= '9' )
		{
			const size_t idx = (size_t)( text[i + 2] - '1' );

			if( idx < args.size() )
				out += args[idx];

			i += 3;
			continue;
		}

		out += text[i++];
	}

	return out;
}

// The text is made ready to be drawn: Windows line ends and the line ends at the end are removed,
// control characters other than line ends become spaces
inline std::string CleanText( const std::string& text )
{
	std::string out;
	out.reserve( text.size() );

	for( char c : text )
	{
		if( c == '\r' )
			continue;

		if( (unsigned char)c < ' ' && c != '\n' )
			c = ' ';

		out += c;
	}

	while( !out.empty() && ( out.back() == '\n' || out.back() == ' ' ))
		out.pop_back();

	return out;
}

// ---- the layout

namespace Size {
const int Margin = 10;      // from the right edge and from the top of the screen (or of the spectator bar)
const int Radius = 10;
const int MinHeight = 45;
const int LeftWidth = 40;   // the part with the icon
const int Cut = 2;          // the cut between the parts
const int IconPos = 10;
const int IconSize = 25;
const int TextX = 50;       // the text starts here
const int TextRight = 17;   // room on the right of the text
const int TextPad = 10;     // room above and below the text when it has many lines (line ends in the text)
} // namespace Size

// The Steam version draws the "tall" of a scheme with letters of tall * 3 / 4 pixels (rounded down, the em):
// the tutor font ("tall" 14) has letters of 10 pixels, capitals of 7 and small letters of 5 (measured on its
// screenshots; the font of the spectator bar gives the same rule, see spectator_layout.h)
inline int FontEmHeight( int schemeTall )
{
	const int em = schemeTall * 3 / 4;
	return em < 4 ? 4 : em;
}

struct Layout
{
	int x = 0, y = 0, w = 0, h = 0;  // the panel
	int iconX = 0, iconY = 0;
	int cutX = 0;                    // the cut is cutX .. cutX + Size::Cut
	int textX = 0, textY = 0;
};

// The longest line that fits on the screen (a longer one is cut at the right edge of the window)
inline int MaxTextWidth( int screenW )
{
	int w = screenW - Size::Margin - Size::TextX - Size::TextRight;
	if( w < 50 )
		w = 50;
	return w;
}

// screenW - width of the screen, top - where the window may start (0, or the bottom of the spectator bar),
// textW / textH - the size of the drawn text
inline Layout ComputeLayout( int screenW, int top, int textW, int textH )
{
	Layout l;

	if( textW < 0 ) textW = 0;
	if( textH < 0 ) textH = 0;

	l.w = Size::TextX + textW + Size::TextRight;
	l.h = textH + Size::TextPad * 2;
	if( l.h < Size::MinHeight )
		l.h = Size::MinHeight;

	l.x = screenW - Size::Margin - l.w;
	if( l.x < 0 )
		l.x = 0;
	l.y = top + Size::Margin;

	l.iconX = l.x + Size::IconPos;
	l.iconY = l.y + Size::IconPos;
	l.cutX = l.x + Size::LeftWidth;
	l.textX = l.x + Size::TextX;

	// in the middle of the panel, as in the Steam version
	l.textY = l.y + ( l.h - textH ) / 2;

	return l;
}

// The text split at its line ends; a line wider than maxWidth is cut (UTF-8 letters are kept whole).
// For the font of the HUD (the Windows fonts cut the lines by themselves).
// width( const std::string& ) gives the width of a piece of text.
template <class WidthFn>
std::vector<std::string> ClipLines( const std::string& text, int maxWidth, WidthFn width )
{
	std::vector<std::string> lines;
	size_t start = 0;

	for( ;; )
	{
		const size_t end = text.find( '\n', start );
		std::string line = text.substr( start, end == std::string::npos ? std::string::npos : end - start );

		while( !line.empty() && width( line ) > maxWidth )
		{
			size_t cut = line.size() - 1;

			while( cut > 0 && ( (unsigned char)line[cut] & 0xC0 ) == 0x80 )
				--cut;

			line.erase( cut );
		}

		lines.push_back( line );

		if( end == std::string::npos )
			break;
		start = end + 1;
	}

	return lines;
}

} // namespace Tutor
