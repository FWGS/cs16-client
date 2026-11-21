#pragma once

// CSWeb logging. Everything goes through the engine console (Con_Printf).
//
// Info is always printed. Debug, Warn, Error and Panic are printed
// only when csweb_debug is 1.

#include "csw_cvars.h"
#include "csw_internal.h"

#include <cstdarg>
#include <cstdio>

namespace csw
{

// Numeric values must match LogLevel in rust/src/console.rs.
enum class LogLevel : int
{
    Info  = 0,
    Debug = 1,
    Warn  = 2,
    Error = 3,
    Panic = 4,
};

namespace detail
{

inline void Vlog( LogLevel level, const char *fmt, va_list args )
{
    static const char *const tags[] =
    {
        "",
        "^5debug^7: ",
        "^3warning^7: ",
        "^1error^7: ",
        "^1PANIC^7: ",
    };

    if( level != LogLevel::Info && DebugLevel() < 1 )
        return;

    char message[2048];
    vsnprintf( message, sizeof( message ), fmt, args );

    gEngfuncs.Con_Printf( "CSWeb: %s%s\n", tags[(int)level], message );
}

inline LogLevel FromInt( int level )
{
    return level >= 0 && level <= (int)LogLevel::Panic ? (LogLevel)level : LogLevel::Debug;
}

} // namespace detail

#define CSW_LOG_FN( name, level ) \
inline void name( const char *fmt, ... ) \
{ \
    va_list args; \
    va_start( args, fmt ); \
    detail::Vlog( level, fmt, args ); \
    va_end( args ); \
}

CSW_LOG_FN( Info, LogLevel::Info )
CSW_LOG_FN( Debug, LogLevel::Debug )
CSW_LOG_FN( Warn, LogLevel::Warn )
CSW_LOG_FN( Error, LogLevel::Error )

#undef CSW_LOG_FN

// Callback handed to the library (ServoViewConfig::log).
inline void LibraryLog( int level, const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    detail::Vlog( detail::FromInt( level ), fmt, args );
    va_end( args );
}

} // namespace csw