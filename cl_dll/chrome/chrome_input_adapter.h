#pragma once

#include "chrome_types.h"

namespace iHTMLChrome {
namespace CEF {
class ChromeInstance;

class InputAdapter
{
public:
	InputEvent MakeMouseMove( int screen_x, int screen_y, const ChromeRect& viewport, int browser_w, int browser_h ) const;
	InputEvent MakeMouseButton( bool down, int button, int screen_x, int screen_y, const ChromeRect& viewport, int browser_w, int browser_h ) const;
	InputEvent MakeMouseWheel( int delta, int screen_x, int screen_y, const ChromeRect& viewport, int browser_w, int browser_h ) const;
	InputEvent MakeKey( bool down, int key ) const;
	InputEvent MakeChar( std::uint32_t codepoint ) const;
	InputEvent MakeTouch( InputType type, int pointer_id, int screen_x, int screen_y, const ChromeRect& viewport, int browser_w, int browser_h ) const;

	void Dispatch( ChromeInstance& instance, const InputEvent& event ) const;

private:
	void ScreenToChrome( int screen_x, int screen_y, const ChromeRect& viewport, int browser_w, int browser_h, int& out_x, int& out_y ) const;
};
} // namespace CEF
} // namespace iHTMLChrome
