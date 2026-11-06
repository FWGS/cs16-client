#pragma once

#if defined(__GNUC__)
#define CHROME_DEBUG_PRINTF_ATTR( fmt_idx, first_arg ) __attribute__( ( format( printf, fmt_idx, first_arg ) ) )
#else
#define CHROME_DEBUG_PRINTF_ATTR( fmt_idx, first_arg )
#endif

void Chrome_DInit();
float Chrome_Level();
void Chrome_DPrintf( float min_level, const char* fmt, ... ) CHROME_DEBUG_PRINTF_ATTR( 2, 3 );
void Chrome_Printf( const char* fmt, ... ) CHROME_DEBUG_PRINTF_ATTR( 1, 2 );

#undef CHROME_DEBUG_PRINTF_ATTR
