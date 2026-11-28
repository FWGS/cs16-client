#pragma once

// Internal CSWeb header: shared state of the client-side glue.
// Do not include from the rest of the client, use csw_interface.h for that.

#include "hud.h"
#include "cl_util.h"
#include "csw_view.h" // 3rdparty/csweb
#include <cstdint>

namespace csw
{

struct Api
{
    uint32_t   ( *abi_version )();
    ServoView *( *create )( const ServoViewConfig * );
    void       ( *destroy )( ServoView * );
    void       ( *resize )( ServoView *, uint32_t, uint32_t );
    void       ( *load_url )( ServoView *, const char * );
    int        ( *acquire )( ServoView *, ServoViewFrame * );
    void       ( *mouse_move )( ServoView *, float, float );
    void       ( *mouse_button )( ServoView *, int, int, float, float );
    void       ( *wheel )( ServoView *, float, float, float, float );
    void       ( *key )( ServoView *, int, uint32_t, int, uint32_t );
    void       ( *set_debug_level )( int );
};

struct State
{
    // loader
    Api        api = {};
    bool       apiReady = false; // true only after all symbols are bound and ABI is checked
    void      *lib = nullptr;    // never unloaded: Servo threads outlive any view
    ServoView *view = nullptr;
    int        debugSynced = -1;

    // render
    int      tex = 0;
    uint32_t texW = 0, texH = 0;
    int      texSerial = 0;
    float    rx1 = 0, ry1 = 0, rx2 = 0, ry2 = 0; // last drawn rect (screen coords)

    // input
    int   lastMx = -1, lastMy = -1; // last real cursor position sent to the page
    bool  mouseSent = false;
    bool  shift = false, ctrl = false, alt = false, caps = false;
};

extern State g; // defined in csw_loader.cpp

} // namespace csw