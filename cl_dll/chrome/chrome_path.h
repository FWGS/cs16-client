#pragma once

// Paths of the embedded browser are kept in UTF-8 strings (CEF takes UTF-8 too).
// std::filesystem::path made from a plain std::string uses the ANSI code page on Windows, and
// generic_string() gives ANSI back, so a folder with letters that are not ASCII (D:\Игры\cs16)
// would be broken. These helpers always convert through UTF-8.

#include <filesystem>
#include <string>

#include "chrome_motd_utf.h"

namespace iHTMLChrome {
namespace CEF {

inline std::filesystem::path PathFromUtf8( const std::string& utf8 )
{
	return std::filesystem::path( std::u8string( reinterpret_cast<const char8_t*>( utf8.data() ), utf8.size() ) );
}

inline std::string PathToUtf8( const std::filesystem::path& path )
{
	const std::u8string u8 = path.generic_u8string();
	return std::string( reinterpret_cast<const char*>( u8.data() ), u8.size() );
}

// A path given by the engine: UTF-8 if it is valid UTF-8, otherwise the ANSI code page of the system
inline std::filesystem::path PathFromEngine( const std::string& s )
{
	if( IsValidUtf8( s.c_str() ) )
		return PathFromUtf8( s );

	return std::filesystem::path( s );
}

} // namespace CEF
} // namespace iHTMLChrome
