#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iHTMLChrome {
namespace CEF {
struct FrameBuffer
{
	int width = 0;
	int height = 0;
	std::uint64_t sequence = 0;
	std::vector<std::uint8_t> rgba;
};

struct ChromeRect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

enum class InputType
{
	MouseMove,
	MouseDown,
	MouseUp,
	MouseWheel,
	KeyDown,
	KeyUp,
	KeyChar,
	TouchDown,
	TouchUp,
	TouchMove
};

// state of the keyboard modifiers and of the mouse buttons (InputEvent::modifiers)
constexpr int kModShift = 1;
constexpr int kModCtrl = 2;
constexpr int kModAlt = 4;
constexpr int kModLeftButton = 8;
constexpr int kModMiddleButton = 16;
constexpr int kModRightButton = 32;

struct InputEvent
{
	InputType type = InputType::MouseMove;
	int x = 0;
	int y = 0;
	int delta_x = 0;
	int delta_y = 0;
	int button = 0; // 0 - left, 1 - middle, 2 - right
	int key = 0;    // Windows key code (or the character for KeyChar)
	std::uint32_t codepoint = 0;
	int pointer_id = 0;
	int modifiers = 0;      // kMod* flags
	int click_count = 1;
	bool mouse_leave = false;
};
} // namespace CEF
} // namespace iHTMLChrome
