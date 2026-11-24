#include "chrome_input_adapter.h"
#include "chrome_instance.h"

namespace iHTMLChrome {
namespace CEF {
InputEvent InputAdapter::MakeMouseMove( int, int, const ChromeRect&, int, int ) const
{
	return InputEvent{};
}

InputEvent InputAdapter::MakeMouseButton( bool, int, int, int, const ChromeRect&, int, int ) const
{
	return InputEvent{};
}

InputEvent InputAdapter::MakeMouseWheel( int, int, int, const ChromeRect&, int, int ) const
{
	return InputEvent{};
}

InputEvent InputAdapter::MakeKey( bool, int ) const
{
	return InputEvent{};
}

InputEvent InputAdapter::MakeChar( std::uint32_t ) const
{
	return InputEvent{};
}

InputEvent InputAdapter::MakeTouch( InputType, int, int, int, const ChromeRect&, int, int ) const
{
	return InputEvent{};
}

void InputAdapter::Dispatch( ChromeInstance&, const InputEvent& ) const
{
}

void InputAdapter::ScreenToChrome(
	int screen_x,
	int screen_y,
	const ChromeRect&,
	int,
	int,
	int& out_x,
	int& out_y ) const
{
	out_x = screen_x;
	out_y = screen_y;
}
} // namespace CEF
} // namespace iHTMLChrome
