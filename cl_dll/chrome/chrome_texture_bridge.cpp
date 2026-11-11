#include "chrome_texture_bridge.h"

#include "wrect.h"
#include "cl_dll.h"
#include "../hud/client_texture.h"

#include <cstdio>
#include <vector>

namespace
{
// The engine gives back the texture it already has for a name, so every texture needs its own name:
// otherwise two browsers would draw into one texture and freeing one would break the other
int g_chrome_texture_counter = 0;

void MakeTextureName( char* name, std::size_t size )
{
	std::snprintf( name, size, "*chrome_offscreen_%d", ++g_chrome_texture_counter );
}
} // namespace

namespace iHTMLChrome {
namespace CEF {
bool TextureBridge::Initialize()
{
	return true;
}

void TextureBridge::Shutdown()
{
	DestroyTexture();
}

unsigned int TextureBridge::texture_id() const
{
	return ClientTexture::Owned( (int)texture_id_, name_ ) ? texture_id_ : 0;
}

void TextureBridge::EnsureSize( int width, int height )
{
	if( width <= 0 || height <= 0 )
		return;

	// the engine has freed the texture: a page must never be drawn into a texture of the engine by the old number
	if( texture_id_ != 0 && !ClientTexture::Owned( (int)texture_id_, name_ ))
	{
		ClientTexture::ReportLost( name_ );
		texture_id_ = 0;
		name_[0] = 0;
	}

	if( texture_id_ == 0 || width_ != width || height_ != height )
		CreateTexture( width, height );
}

void TextureBridge::Upload( const FrameBuffer& frame )
{
	if( frame.width <= 0 || frame.height <= 0 || frame.rgba.empty() )
		return;

	if( gRenderAPI.AVI_UploadRawFrame )
	{
		EnsureSize( frame.width, frame.height );
		if( texture_id_ == 0 )
			return;

		// the engine binds the texture to the texture unit that is active now (GL_KEEP_UNIT): the unit 0 is
		// chosen first, so the page never lands on the unit of the lightmaps or the detail textures
		if( gRenderAPI.GL_SelectTexture )
			gRenderAPI.GL_SelectTexture( 0 );

		gRenderAPI.AVI_UploadRawFrame( texture_id_, frame.width, frame.height, frame.width, frame.height, frame.rgba.data() );
		return;
	}

	DestroyTexture();
	if( !gRenderAPI.GL_CreateTexture )
		return;

	const std::size_t pixels = static_cast<std::size_t>( frame.width ) * static_cast<std::size_t>( frame.height );
	std::vector<unsigned char> rgba;
	rgba.resize( pixels * 4 );
	const unsigned char* bgra = frame.rgba.data();
	for( std::size_t i = 0; i < pixels; ++i )
	{
		const std::size_t s = i * 4;
		const std::size_t d = i * 4;
		rgba[d + 0] = bgra[s + 2];
		rgba[d + 1] = bgra[s + 1];
		rgba[d + 2] = bgra[s + 0];
		rgba[d + 3] = bgra[s + 3];
	}

	MakeTextureName( name_, sizeof( name_ ) );

	texture_id_ = gRenderAPI.GL_CreateTexture(
		name_,
		frame.width,
		frame.height,
		rgba.data(),
		static_cast<texFlags_t>( TF_NOMIPMAP | TF_CLAMP | TF_HAS_ALPHA | TF_NEAREST ) );
	if( (int)texture_id_ <= 0 )
	{
		texture_id_ = 0;
		name_[0] = 0;
	}
	width_ = frame.width;
	height_ = frame.height;
}

void TextureBridge::CreateTexture( int width, int height )
{
	DestroyTexture();

	if( !gRenderAPI.GL_CreateTexture )
		return;

	MakeTextureName( name_, sizeof( name_ ) );

	texture_id_ = gRenderAPI.GL_CreateTexture(
		name_,
		width,
		height,
		nullptr,
		static_cast<texFlags_t>( TF_NOMIPMAP | TF_CLAMP | TF_HAS_ALPHA ) );
	if( (int)texture_id_ <= 0 )
	{
		texture_id_ = 0;
		name_[0] = 0;
	}
	width_ = width;
	height_ = height;
}

void TextureBridge::DestroyTexture()
{
	int tex = (int)texture_id_;
	ClientTexture::Free( tex, name_ );
	name_[0] = 0;
	texture_id_ = 0;
	width_ = 0;
	height_ = 0;
}
} // namespace CEF
} // namespace iHTMLChrome
