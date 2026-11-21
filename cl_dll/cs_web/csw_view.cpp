// View lifetime, frame acquisition and drawing.

#include "csw_internal.h"

#include "triangleapi.h"
#include "render_api.h"
#include "draw_util.h"

#include "csw_interface.h"
#include "csw_cvars.h"
#include "csw_log.h"

#include <cstdio>

using csw::g;

bool CSW_Create( int w, int h, const char *url )
{
    if( !g.apiReady )
    {
        csw::Error( "create failed: library is not ready" );
        return false;
    }

    if( g.view )
    {
        csw::Debug( "create skipped: view already exists" );
        return true;
    }

    if( w <= 0 || h <= 0 )
    {
        csw::Warn( "invalid view size: %dx%d", w, h );
        return false;
    }

    csw::SyncDebug();

    ServoViewConfig cfg = {};
    cfg.abi_version = CS_WEB_ABI_VERSION;
    cfg.width = (uint32_t)w;
    cfg.height = (uint32_t)h;
    cfg.url = url;
    cfg.log = csw::LibraryLog;

    g.view = g.api.create( &cfg );

    if( g.view )
        csw::Info( "view ^2created^7 (%ux%u) url=%s", cfg.width, cfg.height, cfg.url );
    else
        csw::Error( "view creation ^1FAILED^7 (%ux%u) url=%s", cfg.width, cfg.height, cfg.url );

    return g.view != nullptr;
}

void CSW_Destroy()
{
    g.focus = false;

    if( !g.view )
        return;

    g.api.destroy( g.view );
    g.view = nullptr;
    g.tex = 0; // the engine frees its textures on renderer shutdown
}

bool CSW_Active()
{
    return g.view != nullptr;
}

void CSW_Frame()
{
    csw::SyncDebug();

    if( !g.view )
        return;

    ServoViewFrame frame = {};

    if( !g.api.acquire( g.view, &frame ) || !frame.pixels )
        return;

    // A new texture is created when the dimensions change, otherwise the old one is updated.
    const bool recreate = !g.tex || frame.width != g.texW || frame.height != g.texH;
    if( recreate )
        g.texSerial++;

    char name[32];
    snprintf( name, sizeof( name ), "*csw_%d", g.texSerial );

    texFlags_t flags = (texFlags_t)( TF_NOMIPMAP | TF_CLAMP );
    if( !recreate )
        flags = (texFlags_t)( flags | TF_UPDATE );

    int tex = gRenderAPI.GL_CreateTexture( name, frame.width, frame.height, frame.pixels, flags );

    if( recreate )
    {
        g.tex = tex;
        g.texW = frame.width;
        g.texH = frame.height;

        csw::Debug( "texture %s created: %ux%u, id=%d", name, frame.width, frame.height, g.tex );
    }
}

void CSW_Draw( float w, float h, float posX, float posY )
{
    if( !g.tex )
        return;

    g.rx1 = posX;
    g.ry1 = posY;
    g.rx2 = posX + w;
    g.ry2 = posY + h;

    gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
    gEngfuncs.pTriAPI->Color4ub( 255, 255, 255, 255 );
    gRenderAPI.GL_Bind( 0, g.tex );
    DrawUtils::Draw2DQuad( posX, posY, posX + w, posY + h );

    if( g.focus )
    {
        csw::ClampCursor();
        gEngfuncs.pfnFillRGBABlend( (int)g.cx - 3, (int)g.cy - 3, 8, 8, 0, 0, 0, 255 );
        gEngfuncs.pfnFillRGBABlend( (int)g.cx - 2, (int)g.cy - 2, 6, 6, 255, 255, 255, 255 );
    }
}

void CSW_LoadURL( const char *url )
{
    if( g.view )
        g.api.load_url( g.view, url );
}