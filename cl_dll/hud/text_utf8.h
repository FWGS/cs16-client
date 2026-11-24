//
// text_utf8.h
//
// Texts that come from servers are in UTF-8 or in the Windows-1251 code page (Russian servers).
// Anything that is not valid UTF-8 is taken as Windows-1251 and converted to UTF-8.
// No dependencies on the engine.
//

#pragma once

#include <string>

namespace TextUtf {

inline bool IsValidUtf8( const char* s )
{
	const unsigned char* p = reinterpret_cast<const unsigned char*>( s );

	while( *p )
	{
		int n;

		if( *p < 0x80 )
			n = 0;
		else if(( *p & 0xE0 ) == 0xC0 && *p >= 0xC2 )
			n = 1;
		else if(( *p & 0xF0 ) == 0xE0 )
			n = 2;
		else if(( *p & 0xF8 ) == 0xF0 && *p <= 0xF4 )
			n = 3;
		else
			return false;

		for( int i = 1; i <= n; ++i )
			if(( p[i] & 0xC0 ) != 0x80 )
				return false;

		p += n + 1;
	}

	return true;
}

inline void AppendUtf8( std::string& out, unsigned int cp )
{
	if( cp < 0x80 )
	{
		out += (char)cp;
	}
	else if( cp < 0x800 )
	{
		out += (char)( 0xC0 | ( cp >> 6 ));
		out += (char)( 0x80 | ( cp & 0x3F ));
	}
	else
	{
		out += (char)( 0xE0 | ( cp >> 12 ));
		out += (char)( 0x80 | (( cp >> 6 ) & 0x3F ));
		out += (char)( 0x80 | ( cp & 0x3F ));
	}
}

// Anything that is not valid UTF-8 is taken as Windows-1251
inline std::string ToUtf8( const char* s )
{
	if( !s )
		return std::string();

	if( IsValidUtf8( s ))
		return std::string( s );

	static const unsigned short cp1251_80_BF[64] = {
		0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
		0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
		0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
		0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457 };

	std::string out;

	for( const unsigned char* p = reinterpret_cast<const unsigned char*>( s ); *p; ++p )
	{
		if( *p < 0x80 )
			out += (char)*p;
		else if( *p < 0xC0 )
			AppendUtf8( out, cp1251_80_BF[*p - 0x80] );
		else
			AppendUtf8( out, 0x0410 + ( *p - 0xC0 )); // A..ya
	}

	return out;
}

} // namespace TextUtf
