//
// client_texture.h
//
// The textures the client makes itself (the pages of the font atlases) are known by their number and their name.
// The number alone is not enough: if the engine has freed the texture (a restart of the renderer frees all of
// them), the same number may be given to a texture of the engine, for example a font.
// Freeing it, drawing into it or showing it by the old number would break that texture: the texts of the chat
// and the kill feed become black boxes. So the number is used only while the engine has our name for it.
//

#pragma once

#include <string.h>

#include "wrect.h"
#include "cl_dll.h"

namespace ClientTexture {

// is the texture "tex" still the one we made with the name "name"
inline bool Owned( int tex, const char *name )
{
	if( tex <= 0 || !name || !name[0] )
		return false;

	if( gRenderAPI.GL_FindTexture )
		return gRenderAPI.GL_FindTexture( name ) == tex;

	if( gRenderAPI.GL_TextureName )
	{
		const char *current = gRenderAPI.GL_TextureName( (unsigned int)tex );
		return current && !strcmp( current, name );
	}

	return true; // an engine without these functions: trust the number
}

// tells in the console (a few times only) that the engine has freed a texture of the client
inline void ReportLost( const char *name )
{
	static int reported = 0;
	if( reported < 8 && name && name[0] )
	{
		++reported;
		gEngfuncs.Con_Printf( "[client textures] the engine has freed \"%s\", the client makes it again\n", name );
	}
}

// frees the texture only if it is still ours; the number is forgotten anyway
inline void Free( int &tex, const char *name )
{
	if( tex > 0 )
	{
		if( Owned( tex, name ) && gRenderAPI.GL_FreeTexture )
			gRenderAPI.GL_FreeTexture( (unsigned int)tex );
	}

	tex = 0;
}

} // namespace ClientTexture
