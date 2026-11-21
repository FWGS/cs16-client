#pragma once

#include "csw_internal.h"
#include "csw_interface.h"

namespace csw
{

struct Cvars
{
    cvar_t *version = nullptr;
    cvar_t *debug = nullptr;
    cvar_t *cursorScale = nullptr;
};

inline Cvars cv;

inline void RegisterCvars()
{
    static bool done = false;
    if( done )
        return;

    done = true;

    cv.version = CVAR_CREATE( "csweb_ver", CSWEB_VERSION, 0 );
    cv.debug = CVAR_CREATE( "csweb_debug", "0", FCVAR_ARCHIVE );
    cv.cursorScale = CVAR_CREATE( "csweb_cursor_scale", "1", FCVAR_ARCHIVE );
}

// 0 - Info only, 1 - everything (Debug, Warn, Error, Panic, Servo log)
inline int DebugLevel()
{
    return cv.debug ? (int)cv.debug->value : 0;
}

inline float CursorScale()
{
    return cv.cursorScale && cv.cursorScale->value > 0.0f ? cv.cursorScale->value : 1.0f;
}

// Pushes csweb_debug to the library when it changes.
inline void SyncDebug()
{
    if( !g.apiReady )
        return;

    int level = DebugLevel();
    if( level == g.debugSynced )
        return;

    g.debugSynced = level;
    g.api.set_debug_level( level );
}

} // namespace csw