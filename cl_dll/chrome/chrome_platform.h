#pragma once

#if defined(CS16CLIENT_ENABLE_CEF)

#include <string>

#include "cef_app.h"

namespace iHTMLChrome {
namespace CEF {

struct CefEnvironment;

bool Platform_GetExecutablePath( std::string& executable_path );
CefMainArgs Platform_CreateMainArgs();
bool Platform_LoadCEF( const CefEnvironment& env );

} // namespace CEF
} // namespace iHTMLChrome

#endif
