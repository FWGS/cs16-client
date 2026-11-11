#pragma once

#if defined(CS16CLIENT_ENABLE_CEF)

#include "cef_app.h"

namespace iHTMLChrome {
namespace CEF {

struct CefEnvironment;

CefSettings BuildCefSettings( const CefEnvironment& env );

} // namespace CEF
} // namespace iHTMLChrome

#endif
