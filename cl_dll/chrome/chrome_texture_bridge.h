#pragma once

#include "chrome_types.h"

namespace iHTMLChrome {
namespace CEF {
class TextureBridge
{
public:
	bool Initialize();
	void Shutdown();

	void EnsureSize( int width, int height );
	void Upload( const FrameBuffer& frame );

	// 0 if the engine has freed the texture (its number may belong to another texture then)
	unsigned int texture_id() const;
	int width() const { return width_; }
	int height() const { return height_; }

private:
	void CreateTexture( int width, int height );
	void DestroyTexture();

	unsigned int texture_id_ = 0;
	char name_[48] = "";      // the name of the texture in the engine (see hud/client_texture.h)
	int width_ = 0;
	int height_ = 0;
};
} // namespace CEF
} // namespace iHTMLChrome
