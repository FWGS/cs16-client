#pragma once

#if defined(CS16CLIENT_ENABLE_CEF)

#include "cef_base.h"
#include "cef_command_line.h"

namespace iHTMLChrome {
namespace CEF {

// true: Chromium runs inside the game process (used when there is no helper executable)
void SetCefSingleProcess( bool enable );

void ApplyCefCommandLinePolicy( CefRefPtr<CefCommandLine> command_line );

} // namespace CEF
} // namespace iHTMLChrome

#endif
