// Virtual cursor, focus and keyboard / mouse forwarding.

#include "csw_internal.h"
#include "keydefs.h"
#include "csw_interface.h"
#include "csw_cvars.h"
#include "csw_log.h"

using csw::g;

namespace csw
{

void ClampCursor()
{
    if( g.cx < g.rx1 ) g.cx = g.rx1;
    if( g.cx > g.rx2 ) g.cx = g.rx2;
    if( g.cy < g.ry1 ) g.cy = g.ry1;
    if( g.cy > g.ry2 ) g.cy = g.ry2;
}

} // namespace csw

// Screen coordinates -> page coordinates.
static bool ToPage( float sx, float sy, float &px, float &py )
{
    if( !g.view || !g.texW || g.rx2 <= g.rx1 || g.ry2 <= g.ry1 )
        return false;

    px = ( sx - g.rx1 ) / ( g.rx2 - g.rx1 ) * (float)g.texW;
    py = ( sy - g.ry1 ) / ( g.ry2 - g.ry1 ) * (float)g.texH;
    return true;
}

static void Wheel( float dy )
{
    float px, py;
    if( ToPage( g.cx, g.cy, px, py ) )
        g.api.wheel( g.view, 0.0f, dy, px, py );
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

bool CSW_HasFocus()
{
    return g.focus && g.view;
}

void CSW_SetFocus( bool focus )
{
    if( focus && !g.view )
        return;

    if( focus && !g.focus )
    {
        // start in the middle of the page
        g.cx = ( g.rx1 + g.rx2 ) * 0.5f;
        g.cy = ( g.ry1 + g.ry2 ) * 0.5f;
    }

    g.focus = focus;
    csw::Debug( "focus %s", focus ? "^2ON^7 ^5(Esc to leave)^7" : "^1OFF^7" );
}

void CSW_LookEvent( float relyaw, float relpitch )
{
    if( !g.view )
        return;

    // yaw -= x * m_yaw, pitch += y * m_pitch.
    float myaw = gEngfuncs.pfnGetCvarFloat( "m_yaw" );
    float mpitch = gEngfuncs.pfnGetCvarFloat( "m_pitch" );
    if( myaw == 0.0f ) myaw = 0.022f;
    if( mpitch == 0.0f ) mpitch = 0.022f;

    const float scale = csw::CursorScale();
    g.cx += ( -relyaw / myaw ) * scale;
    g.cy += ( relpitch / mpitch ) * scale;
    csw::ClampCursor();

    float px, py;
    if( ToPage( g.cx, g.cy, px, py ) )
        g.api.mouse_move( g.view, px, py );
}

void CSW_MouseButton( int button, bool down )
{
    float px, py;
    if( ToPage( g.cx, g.cy, px, py ) )
        g.api.mouse_button( g.view, button, down ? 1 : 0, px, py );
}

bool CSW_Key( int down, int key )
{
    // Modifier state is tracked even without focus, so it is right the moment focus starts.
    if( key == K_SHIFT ) g.shift = down != 0;
    else if( key == K_CTRL ) g.ctrl = down != 0;
    else if( key == K_ALT ) g.alt = down != 0;
    else if( key == K_CAPSLOCK && down ) g.caps = !g.caps;

    if( !CSW_HasFocus() )
        return false;

    if( key == K_ESCAPE )
    {
        if( down )
            CSW_SetFocus( false );
        return true; // swallow Esc so the game menu does not open
    }

    if( key == K_SHIFT || key == K_CTRL || key == K_ALT || key == K_CAPSLOCK )
        return true;

    if( key == K_MWHEELUP || key == K_MWHEELDOWN )
    {
        if( down )
            Wheel( key == K_MWHEELUP ? 3.0f : -3.0f ); // positive = scroll up
        return true;
    }

    const uint32_t mods = ( g.shift ? 1u : 0u ) | ( g.ctrl ? 2u : 0u ) | ( g.alt ? 4u : 0u );
    const uint32_t cp = PrintableKey( key );
    const int special = cp ? 0 : SpecialKey( key );

    if( cp || special )
        g.api.key( g.view, down ? 1 : 0, cp, special, mods );

    return true; // everything is swallowed while the page has focus
}