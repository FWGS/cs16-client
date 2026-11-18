#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "chrome_backend.h"
#include "chrome_types.h"
#include "chrome_texture_bridge.h"

namespace iHTMLChrome {
namespace CEF {

using ChromePaintConsumerFn =
	void (*)( void* userdata, const std::uint8_t* rgba, int width, int height );

class ChromeInstance
{
public:
	explicit ChromeInstance( int id );
	~ChromeInstance();

	bool Initialize( const char* start_url = nullptr );
	void Shutdown();

	void SetPaintConsumer( ChromePaintConsumerFn consumer, void* userdata );

	void LoadURL( const std::string& url );
	void Resize( int width, int height );
	void Tick();

	void EnqueueInput( const InputEvent& event );
	void OnCefPaint( const std::uint8_t* rgba, int width, int height, std::uint64_t sequence );

	int id() const { return id_; }
	const std::string& url() const { return url_; }
	const ChromeRect& viewport() const { return viewport_; }
	void set_viewport( const ChromeRect& viewport ) { viewport_ = viewport; }
	int width() const { return width_; }
	int height() const { return height_; }
	unsigned int texture_id() const { return texture_bridge_.texture_id(); }

private:
	void UploadLatestFrame();

	int id_ = -1;
	std::string url_;
	ChromeRect viewport_;
	int width_ = 0;
	int height_ = 0;

	TextureBridge texture_bridge_;
	std::atomic<bool> initialized_{ false };

	std::mutex frame_mutex_;
	FrameBuffer pending_frame_;
	FrameBuffer staged_frame_;
	bool has_pending_frame_ = false;

	std::mutex input_mutex_;
	std::queue<InputEvent> pending_input_;
	std::unique_ptr<ChromeBackend> backend_;

	ChromePaintConsumerFn paint_consumer_ = nullptr;
	void* paint_consumer_userdata_ = nullptr;
};
} // namespace CEF
} // namespace iHTMLChrome
