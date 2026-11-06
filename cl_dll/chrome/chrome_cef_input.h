#pragma once

// Translation of the input events into calls of the CEF browser host.
// The CEF headers (CefMouseEvent, CefKeyEvent, ...) must be included before this file.

#include <cstdint>

#include "chrome_types.h"

namespace iHTMLChrome {
namespace CEF {

template <class HostPtr>
inline void DispatchInputToCef( HostPtr& host, const InputEvent& event )
{
	std::uint32_t flags = 0;
	if( event.modifiers & kModShift )
		flags |= EVENTFLAG_SHIFT_DOWN;
	if( event.modifiers & kModCtrl )
		flags |= EVENTFLAG_CONTROL_DOWN;
	if( event.modifiers & kModAlt )
		flags |= EVENTFLAG_ALT_DOWN;
	if( event.modifiers & kModLeftButton )
		flags |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	if( event.modifiers & kModMiddleButton )
		flags |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	if( event.modifiers & kModRightButton )
		flags |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

	CefMouseEvent mouse;
	mouse.x = event.x;
	mouse.y = event.y;
	mouse.modifiers = flags;

	switch( event.type )
	{
	case InputType::MouseMove:
		host->SendMouseMoveEvent( mouse, event.mouse_leave );
		break;

	case InputType::MouseDown:
	case InputType::MouseUp:
	{
		cef_mouse_button_type_t button = MBT_LEFT;
		if( event.button == 1 )
			button = MBT_MIDDLE;
		else if( event.button == 2 )
			button = MBT_RIGHT;

		if( event.type == InputType::MouseDown )
			host->SetFocus( true );

		host->SendMouseClickEvent( mouse, button, event.type == InputType::MouseUp, event.click_count > 0 ? event.click_count : 1 );
		break;
	}

	case InputType::MouseWheel:
		host->SendMouseWheelEvent( mouse, event.delta_x, event.delta_y );
		break;

	case InputType::KeyDown:
	case InputType::KeyUp:
	case InputType::KeyChar:
	{
		CefKeyEvent key;
		key.type = event.type == InputType::KeyDown ? KEYEVENT_RAWKEYDOWN
			: event.type == InputType::KeyUp ? KEYEVENT_KEYUP : KEYEVENT_CHAR;
		key.modifiers = flags;
		key.windows_key_code = event.key;
		key.native_key_code = 0;
		key.is_system_key = 0;
		key.character = static_cast<char16_t>( event.codepoint );
		key.unmodified_character = static_cast<char16_t>( event.codepoint );
		key.focus_on_editable_field = 0;
		host->SendKeyEvent( key );
		break;
	}

	default:
		break;
	}
}

} // namespace CEF
} // namespace iHTMLChrome
