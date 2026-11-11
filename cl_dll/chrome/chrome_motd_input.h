#pragma once

// Input logic of the server MOTD window shown by the embedded browser.
//
// This file does not depend on the engine or on CEF, so it can be tested alone.
// The host feeds it with the mouse movement and the key events, and executes
// what it returns: events for the browser, "close the window", "the key must not
// reach the game".

#include "chrome_types.h"

namespace iHTMLChrome {
namespace CEF {

// Keys of the engine (engine/keydefs.h)
namespace MotdKey {
constexpr int Tab = 9, Enter = 13, Escape = 27, Space = 32, Backspace = 127;
constexpr int Up = 128, Down = 129, Left = 130, Right = 131;
constexpr int Alt = 132, Ctrl = 133, Shift = 134;
constexpr int Ins = 147, Del = 148, PgDn = 149, PgUp = 150, Home = 151, End = 152;
constexpr int KpEnter = 169;
constexpr int WheelDown = 239, WheelUp = 240;
constexpr int Mouse1 = 241, Mouse2 = 242, Mouse3 = 243;
} // namespace MotdKey

// Where the window is on the screen (filled by the host every frame)
struct MotdLayout
{
	bool valid = false;
	float x = 0.f, y = 0.f; // page rectangle on the screen
	float w = 0.f, h = 0.f;
	float scale = 1.f;      // screen size / page size
	float close_x = 0.f, close_y = 0.f, close_w = 0.f, close_h = 0.f; // close (OK) button
};

// Result of one event
struct MotdOutput
{
	bool consumed = false; // the game must not see this key
	bool close = false;    // close the window
	int count = 0;
	InputEvent events[4];

	void Add( const InputEvent& e )
	{
		if( count < 4 )
			events[count++] = e;
	}
};

// Translation of an engine key into the Windows key code and the typed character
// Returns false if the key is not forwarded to the page.
inline bool MapMotdKey( int key, bool shift, int& vk, int& ch )
{
	vk = 0;
	ch = 0;

	if( key >= 'a' && key <= 'z' )
	{
		vk = key - 'a' + 'A';
		ch = shift ? vk : key;
		return true;
	}

	if( key >= '0' && key <= '9' )
	{
		static const char shifted[] = ")!@#$%^&*(";
		vk = key;
		ch = shift ? shifted[key - '0'] : key;
		return true;
	}

	switch( key )
	{
	case MotdKey::Space:     vk = 0x20; ch = ' '; return true;
	case MotdKey::Enter:
	case MotdKey::KpEnter:   vk = 0x0D; ch = '\r'; return true;
	case MotdKey::Tab:       vk = 0x09; return true;
	case MotdKey::Backspace: vk = 0x08; ch = 8; return true;
	case MotdKey::Up:        vk = 0x26; return true;
	case MotdKey::Down:      vk = 0x28; return true;
	case MotdKey::Left:      vk = 0x25; return true;
	case MotdKey::Right:     vk = 0x27; return true;
	case MotdKey::Ins:       vk = 0x2D; return true;
	case MotdKey::Del:       vk = 0x2E; return true;
	case MotdKey::PgDn:      vk = 0x22; return true;
	case MotdKey::PgUp:      vk = 0x21; return true;
	case MotdKey::Home:      vk = 0x24; return true;
	case MotdKey::End:       vk = 0x23; return true;
	case ';':  vk = 0xBA; ch = shift ? ':' : ';';  return true;
	case '=':  vk = 0xBB; ch = shift ? '+' : '=';  return true;
	case ',':  vk = 0xBC; ch = shift ? '<' : ',';  return true;
	case '-':  vk = 0xBD; ch = shift ? '_' : '-';  return true;
	case '.':  vk = 0xBE; ch = shift ? '>' : '.';  return true;
	case '/':  vk = 0xBF; ch = shift ? '?' : '/';  return true;
	case '[':  vk = 0xDB; ch = shift ? '{' : '[';  return true;
	case '\\': vk = 0xDC; ch = shift ? '|' : '\\'; return true;
	case ']':  vk = 0xDD; ch = shift ? '}' : ']';  return true;
	case '\'': vk = 0xDE; ch = shift ? '"' : '\''; return true;
	default: break;
	}

	// '`' / '~' (the console key), F1..F12 and others are left to the engine
	return false;
}

class MotdInput
{
public:
	// a new window was opened
	void Open( int screen_w, int screen_h )
	{
		screen_w_ = screen_w > 0 ? screen_w : 1;
		screen_h_ = screen_h > 0 ? screen_h : 1;
		cursor_x_ = screen_w_ * 0.5f;
		cursor_y_ = screen_h_ * 0.5f;
		buttons_ = 0;
		mods_ = 0;
		hover_ = false;
		last_click_time_ = -1000.0;
		click_count_ = 0;
		for( int i = 0; i < 256; ++i )
			swallowed_[i] = false;
	}

	void SetScreen( int w, int h )
	{
		if( w > 0 )
			screen_w_ = w;
		if( h > 0 )
			screen_h_ = h;
		Clamp();
	}

	void SetLayout( const MotdLayout& layout ) { layout_ = layout; }

	float cursor_x() const { return cursor_x_; }
	float cursor_y() const { return cursor_y_; }
	int buttons() const { return buttons_; }
	bool cursor_over_close() const { return OverClose( cursor_x_, cursor_y_ ); }

	// mouse moved (in screen pixels)
	MotdOutput OnMouseDelta( float dx, float dy )
	{
		MotdOutput out;
		out.consumed = true;

		cursor_x_ += dx;
		cursor_y_ += dy;
		Clamp();

		if( dx == 0.f && dy == 0.f )
			return out;

		int px, py;
		const bool inside = PagePoint( cursor_x_, cursor_y_, px, py );

		if( inside )
		{
			InputEvent e;
			e.type = InputType::MouseMove;
			e.x = px;
			e.y = py;
			e.modifiers = Modifiers();
			out.Add( e );
			hover_ = true;
		}
		else if( hover_ )
		{
			InputEvent e;
			e.type = InputType::MouseMove;
			e.x = last_x_;
			e.y = last_y_;
			e.modifiers = Modifiers();
			e.mouse_leave = true;
			out.Add( e );
			hover_ = false;
		}

		if( inside )
		{
			last_x_ = px;
			last_y_ = py;
		}

		return out;
	}

	// a key or a mouse button of the engine; now is the time in seconds
	MotdOutput OnKey( int down, int key, double now )
	{
		MotdOutput out;

		if( key < 0 || key >= 256 )
			return out;

		// modifiers are tracked and passed to the engine as well
		if( key == MotdKey::Shift || key == MotdKey::Ctrl || key == MotdKey::Alt )
		{
			const int bit = key == MotdKey::Shift ? kModShift : key == MotdKey::Ctrl ? kModCtrl : kModAlt;
			if( down ) mods_ |= bit; else mods_ &= ~bit;
			return out;
		}

		if( !down )
		{
			if( !swallowed_[key] )
				return out;

			swallowed_[key] = false;
			out.consumed = true;

			if( key == MotdKey::Mouse1 || key == MotdKey::Mouse2 || key == MotdKey::Mouse3 )
			{
				const int idx = key - MotdKey::Mouse1;
				if( buttons_ & ButtonBit( idx ))
				{
					buttons_ &= ~ButtonBit( idx );
					InputEvent e;
					e.type = InputType::MouseUp;
					e.button = idx;
					e.x = last_x_;
					e.y = last_y_;
					e.modifiers = Modifiers();
					int px, py;
					if( PagePoint( cursor_x_, cursor_y_, px, py ))
					{
						e.x = px;
						e.y = py;
					}
					out.Add( e );
				}
			}
			else if( key != MotdKey::WheelUp && key != MotdKey::WheelDown && key != MotdKey::Enter && key != MotdKey::KpEnter )
			{
				AddKey( out, false, key );
			}

			return out;
		}

		// ---- key pressed ----

		// Enter closes the window, like the default OK button (Esc is left to the engine)
		if( key == MotdKey::Enter || key == MotdKey::KpEnter )
		{
			swallowed_[key] = true;
			out.consumed = true;
			out.close = true;
			return out;
		}

		if( key == MotdKey::Mouse1 || key == MotdKey::Mouse2 || key == MotdKey::Mouse3 )
		{
			swallowed_[key] = true;
			out.consumed = true;

			const int idx = key - MotdKey::Mouse1;

			if( idx == 0 && OverClose( cursor_x_, cursor_y_ ))
			{
				out.close = true;
				return out;
			}

			int px, py;
			if( PagePoint( cursor_x_, cursor_y_, px, py ))
			{
				buttons_ |= ButtonBit( idx );

				if( idx == 0 && now - last_click_time_ < 0.4 && Near( px, py ))
					click_count_ = click_count_ < 3 ? click_count_ + 1 : 1;
				else
					click_count_ = 1;

				last_click_time_ = now;
				last_x_ = px;
				last_y_ = py;

				InputEvent e;
				e.type = InputType::MouseDown;
				e.button = idx;
				e.x = px;
				e.y = py;
				e.click_count = idx == 0 ? click_count_ : 1;
				e.modifiers = Modifiers();
				out.Add( e );
			}

			return out;
		}

		if( key == MotdKey::WheelUp || key == MotdKey::WheelDown )
		{
			swallowed_[key] = true;
			out.consumed = true;

			int px, py;
			if( PagePoint( cursor_x_, cursor_y_, px, py ))
			{
				InputEvent e;
				e.type = InputType::MouseWheel;
				e.x = px;
				e.y = py;
				e.delta_y = key == MotdKey::WheelUp ? 120 : -120;
				e.modifiers = Modifiers();
				out.Add( e );
			}

			return out;
		}

		int vk, ch;
		if( MapMotdKey( key, ( mods_ & kModShift ) != 0, vk, ch ))
		{
			swallowed_[key] = true;
			out.consumed = true;
			AddKey( out, true, key );
		}

		return out;
	}

private:
	static int ButtonBit( int idx ) { return idx == 0 ? kModLeftButton : idx == 1 ? kModMiddleButton : kModRightButton; }

	int Modifiers() const { return mods_ | ( buttons_ & ( kModLeftButton | kModMiddleButton | kModRightButton )); }

	void Clamp()
	{
		if( cursor_x_ < 0.f ) cursor_x_ = 0.f;
		if( cursor_y_ < 0.f ) cursor_y_ = 0.f;
		if( cursor_x_ > screen_w_ - 1 ) cursor_x_ = (float)( screen_w_ - 1 );
		if( cursor_y_ > screen_h_ - 1 ) cursor_y_ = (float)( screen_h_ - 1 );
	}

	bool PagePoint( float sx, float sy, int& px, int& py ) const
	{
		if( !layout_.valid || layout_.scale <= 0.f )
			return false;

		if( sx < layout_.x || sy < layout_.y || sx >= layout_.x + layout_.w || sy >= layout_.y + layout_.h )
			return false;

		px = (int)(( sx - layout_.x ) / layout_.scale );
		py = (int)(( sy - layout_.y ) / layout_.scale );
		return true;
	}

	bool OverClose( float sx, float sy ) const
	{
		return layout_.valid && sx >= layout_.close_x && sy >= layout_.close_y
			&& sx < layout_.close_x + layout_.close_w && sy < layout_.close_y + layout_.close_h;
	}

	bool Near( int px, int py ) const
	{
		const int dx = px - last_x_, dy = py - last_y_;
		return dx * dx + dy * dy <= 16;
	}

	void AddKey( MotdOutput& out, bool down, int key ) const
	{
		int vk, ch;
		if( !MapMotdKey( key, ( mods_ & kModShift ) != 0, vk, ch ))
			return;

		InputEvent e;
		e.type = down ? InputType::KeyDown : InputType::KeyUp;
		e.key = vk;
		e.modifiers = mods_;
		out.Add( e );

		// printable characters are typed, but not for Ctrl / Alt combinations
		if( down && ch && !( mods_ & ( kModCtrl | kModAlt )))
		{
			InputEvent c;
			c.type = InputType::KeyChar;
			c.key = ch;
			c.codepoint = (std::uint32_t)ch;
			c.modifiers = mods_;
			out.Add( c );
		}
	}

	int screen_w_ = 1, screen_h_ = 1;
	float cursor_x_ = 0.f, cursor_y_ = 0.f;
	int buttons_ = 0, mods_ = 0;
	bool hover_ = false;
	int last_x_ = 0, last_y_ = 0;
	double last_click_time_ = -1000.0;
	int click_count_ = 0;
	bool swallowed_[256] = {};
	MotdLayout layout_;
};

} // namespace CEF
} // namespace iHTMLChrome
