#pragma once

// Text helpers of the server MOTD window: server names come in UTF-8 or in the Windows-1251 code page.
// The code is shared with the other texts of the client (cl_dll/hud/text_utf8.h).
// This file has no dependencies on the engine.

#include "../hud/text_utf8.h"

namespace iHTMLChrome {
namespace CEF {

using TextUtf::IsValidUtf8;
using TextUtf::AppendUtf8;
using TextUtf::ToUtf8;

} // namespace CEF
} // namespace iHTMLChrome
