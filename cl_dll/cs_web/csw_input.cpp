// Mouse / keyboard forwarding. The cursor position comes from the engine
// (see CHudMOTD::Draw), there is no virtual cursor.

#include "csw_internal.h"
#include "csw_interface.h"
#include "csw_cvars.h"
#include "keydefs.h"

using csw::g;

// Screen coordinates -> page coordinates.
static bool ToPage( float sx, float sy, float &px, float &py )
{
    if( !g.view || !g.texW || g.rx2 <= g.rx1 || g.ry2 <= g.ry1 )
        return false;

    if( sx < g.rx1 || sx > g.rx2 || sy < g.ry1 || sy > g.ry2 )
        return false;

    px = ( sx - g.rx1 ) / ( g.rx2 - g.rx1 ) * (float)g.texW;
    py = ( sy - g.ry1 ) / ( g.ry2 - g.ry1 ) * (float)g.texH;
    return true;
}


static bool InPage( float sx, float sy )
{
    return sx >= g.rx1 && sx <= g.rx2 && sy >= g.ry1 && sy <= g.ry2;
}

static bool CursorToPage( float &px, float &py )
{
    return ToPage( (float)g.lastMx, (float)g.lastMy, px, py );
}

// Numbering is shared with special_key() in rust/src/input.rs.
static int SpecialKey( int key )
{
    switch( key )
    {
    case K_ENTER: case K_KP_ENTER: return 1;
    case K_BACKSPACE: return 2;
    case K_TAB: return 3;
    case K_UPARROW: return 5;
    case K_DOWNARROW: return 6;
    case K_LEFTARROW: return 7;
    case K_RIGHTARROW: return 8;
    case K_HOME: return 9;
    case K_END: return 10;
    case K_PGUP: return 11;
    case K_PGDN: return 12;
    case K_INS: return 13;
    case K_DEL: return 14;
    }
    return 0;
}

// Engine key numbers are lowercase ASCII for printable keys. US layout only.
static uint32_t PrintableKey( int key )
{
    if( key < 32 || key > 126 )
        return 0;

    if( key >= 'a' && key <= 'z' )
        return ( g.shift != g.caps ) ? (uint32_t)( key - 'a' + 'A' ) : (uint32_t)key;

    if( g.shift )
    {
        static const char from[] = "`1234567890-=[]\\;',./";
        static const char to[]   = "~!@#$%^&*()_+{}|:\"<>?";

        for( int i = 0; from[i]; i++ )
        {
            if( from[i] == key )
                return (uint32_t)to[i];
        }
    }

    return (uint32_t)key;
}

void CSW_MouseMove( int sx, int sy )
{
    if( !g.view )
        return;

    const bool moved = sx != g.lastMx || sy != g.lastMy;
    g.lastMx = sx;
    g.lastMy = sy;

    float px, py;
    if( !ToPage( (float)sx, (float)sy, px, py ) )
    {
        g.mouseSent = false; // outside of the page
        return;
    }

    if( !moved && g.mouseSent )
        return;

    g.mouseSent = true;
    g.api.mouse_move( g.view, px, py );
}

void CSW_MouseButton( int button, bool down )
{
    float px, py;
    if( CursorToPage( px, py ) )
        g.api.mouse_button( g.view, button, down ? 1 : 0, px, py );
}

void CSW_Scroll( float dy )
{
    float px, py;
    if( CursorToPage( px, py ) )
        g.api.wheel( g.view, 0.0f, dy * csw::ScrollScale(), px, py );
}

bool CSW_Key( int down, int key )
{
    if( key == K_SHIFT ) g.shift = down != 0;
    else if( key == K_CTRL ) g.ctrl = down != 0;
    else if( key == K_ALT ) g.alt = down != 0;
    else if( key == K_CAPSLOCK && down ) g.caps = !g.caps;

    if( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 )
    {
        int button = ( key == K_MOUSE1 ) ? 0 : ( key == K_MOUSE2 ) ? 2 : 1;
        CSW_MouseButton( button, down != 0 );
        return down != 0;
    }

    if( key == K_SHIFT || key == K_CTRL || key == K_ALT || key == K_CAPSLOCK )
        return true;

    if( key == K_MWHEELUP || key == K_MWHEELDOWN )
    {
        if( down )
            CSW_Scroll( key == K_MWHEELUP ? 15.0f : -15.0f ); // positive = scroll up
        return true;
    }

    const uint32_t mods = ( g.shift ? 1u : 0u ) | ( g.ctrl ? 2u : 0u ) | ( g.alt ? 4u : 0u );
    const uint32_t cp = PrintableKey( key );
    const int special = cp ? 0 : SpecialKey( key );

    if( cp || special )
        g.api.key( g.view, down ? 1 : 0, cp, special, mods );

    return true;
}