//
// motd_util.h
//
// Helpers of the server MOTD: recognising HTML, finding the address of an iframe, making an address
// safe to give to the system. No dependencies on the engine, so the same file is used by every build
// of the client and can be tested alone.
//

#pragma once

#include <stddef.h>
#include <string.h>

namespace MotdUtil {

inline char LowerAscii( char c )
{
	return ( c >= 'A' && c <= 'Z' ) ? (char)( c + 'a' - 'A' ) : c;
}

// prefix must be in lower case
inline bool StartsWithNoCase( const char *s, const char *prefix )
{
	while( *prefix )
	{
		if( LowerAscii( *s++ ) != *prefix++ )
			return false;
	}

	return true;
}

inline bool IsBlank( char c )
{
	return (unsigned char)c <= ' ';
}

inline bool IsAsciiLetter( char c )
{
	return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
}

inline bool IsHttpUrl( const char *s )
{
	return StartsWithNoCase( s, "http://" ) || StartsWithNoCase( s, "https://" );
}

// The text of a MOTD is HTML if it starts with a tag: "<html", "<!DOCTYPE", "<b>", "</p>"...
// A text like "<<< Welcome >>>" or "<-- rules" is not HTML and is shown in the game.
inline bool LooksLikeHtml( const char *s )
{
	while( *s && IsBlank( *s ))
		s++;

	if( s[0] != '<' )
		return false;

	if( IsAsciiLetter( s[1] ) || s[1] == '!' || s[1] == '?' )
		return true;

	return s[1] == '/' && IsAsciiLetter( s[2] );
}

// the tag <name ...> starts at s ('<' included); name in lower case
inline bool IsTagStart( const char *s, const char *name )
{
	if( s[0] != '<' || !StartsWithNoCase( s + 1, name ))
		return false;

	const char after = s[1 + strlen( name )];
	return IsBlank( after ) || after == '>' || after == '/';
}

// first occurrence of what (lower case) in s, ignoring the case; NULL if there is none
inline const char *FindNoCase( const char *s, const char *what )
{
	for( ; *s; s++ )
		if( StartsWithNoCase( s, what ))
			return s;

	return NULL;
}

// Reads the attributes of a tag that starts at p (right after its name), the way a browser does:
// with quotes or without, '>' can be inside quotes, the first src counts.
// Returns: 1 - an http(s) src was found and copied to out, 0 - the tag has no usable src (end is set
// to the end of the tag), -1 - the tag is not closed (the rest of the page is swallowed by it).
inline int ReadIframeSrc( const char *p, char *out, size_t outSize, const char **end )
{
	for( ;; )
	{
		while( *p && ( IsBlank( *p ) || *p == '/' ))
			p++;

		if( !*p )
			return -1;

		if( *p == '>' )
		{
			*end = p + 1;
			return 0;
		}

		// name of the attribute
		const char *name = p;

		while( *p && !IsBlank( *p ) && *p != '=' && *p != '>' && *p != '/' )
			p++;

		const size_t nameLen = (size_t)( p - name );

		while( *p && IsBlank( *p ))
			p++;

		if( *p != '=' )
			continue; // an attribute without a value

		p++;

		while( *p && IsBlank( *p ))
			p++;

		// the value
		char quote = 0;

		if( *p == '"' || *p == '\'' )
			quote = *p++;

		const char *value = p;

		while( *p && ( quote ? ( *p != quote ) : ( !IsBlank( *p ) && *p != '>' )))
			p++;

		if( !*p )
			return -1;

		const char *valueEnd = p;

		if( quote )
			p++; // the closing quote

		if( nameLen != 3 || !StartsWithNoCase( name, "src" ))
			continue;

		// blanks around the address are not a part of it
		while( value < valueEnd && IsBlank( *value ))
			value++;

		while( valueEnd > value && IsBlank( valueEnd[-1] ))
			valueEnd--;

		size_t len = 0;
		bool ok = true;

		for( const char *q = value; q < valueEnd; q++ )
		{
			if( IsBlank( *q )) // an address never contains blanks or control characters
			{
				ok = false;
				break;
			}

			char c = *q;

			if( valueEnd - q >= 5 && !strncmp( q, "&amp;", 5 )) // HTML escaping of '&'
			{
				c = '&';
				q += 4;
			}

			if( len + 1 >= outSize )
			{
				ok = false;
				break;
			}

			out[len++] = c;
		}

		if( ok )
		{
			out[len] = 0;

			if( IsHttpUrl( out ))
				return 1;
		}

		// the first src of a tag decides: skip the rest of this tag
		for( ;; )
		{
			while( *p && *p != '>' && *p != '"' && *p != '\'' )
				p++;

			if( !*p )
				return -1;

			if( *p == '>' )
			{
				*end = p + 1;
				return 0;
			}

			const char *closing = strchr( p + 1, *p );

			if( !closing )
				return -1;

			p = closing + 1;
		}
	}
}

// Finds the first <iframe ... src="http(s)://..."> of a page and copies its address to out.
// Comments (<!-- -->), scripts and styles are skipped: an iframe written there is not on the page.
// Returns false if there is no such iframe.
inline bool ExtractIframeSrc( const char *html, char *out, size_t outSize )
{
	if( !html || !out || outSize == 0 )
		return false;

	const char *p = html;

	while(( p = strchr( p, '<' )) != NULL )
	{
		if( !strncmp( p, "<!--", 4 ))
		{
			const char *close = strstr( p + 4, "-->" );

			if( !close )
				return false; // the rest of the page is a comment
			p = close + 3;
			continue;
		}

		if( IsTagStart( p, "script" ) || IsTagStart( p, "style" ))
		{
			const char *close = FindNoCase( p + 1, IsTagStart( p, "script" ) ? "</script" : "</style" );

			if( !close )
				return false; // the rest of the page is a script
			p = close + 2;
			continue;
		}

		if( IsTagStart( p, "iframe" ))
		{
			const char *end = NULL;
			const int r = ReadIframeSrc( p + 7, out, outSize, &end );

			if( r == 1 )
				return true;
			if( r < 0 )
				return false;

			p = end;
			continue;
		}

		p++;
	}

	return false;
}

// Makes an http(s) address of a server safe to give to the system browser: everything that is not
// a plain character of an address (quotes, '<', '>', '\', '^', '`', '{', '|', '}', letters that are
// not ASCII) is written as %XX, so the address cannot break out of the command line of the browser.
// Returns false if it is not an http(s) address, has control characters or does not fit into out.
inline bool MakeSafeUrl( const char *in, char *out, size_t outSize )
{
	static const char hex[] = "0123456789ABCDEF";

	if( !in || !out || outSize == 0 || !IsHttpUrl( in ))
		return false;

	size_t len = 0;

	for( const unsigned char *p = (const unsigned char *)in; *p; p++ )
	{
		const unsigned char c = *p;

		if( c <= ' ' || c == 0x7F )
			return false;

		const bool plain = ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' )
			|| ( c < 0x80 && strchr( "-._~:/?#[]@!$&'()*+,;=%", (char)c ));

		if( plain )
		{
			if( len + 1 >= outSize )
				return false;
			out[len++] = (char)c;
		}
		else
		{
			if( len + 3 >= outSize )
				return false;
			out[len++] = '%';
			out[len++] = hex[c >> 4];
			out[len++] = hex[c & 15];
		}
	}

	out[len] = 0;
	return true;
}

} // namespace MotdUtil
