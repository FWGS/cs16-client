#pragma once

#if defined(CS16CLIENT_ENABLE_CEF)

#include <string>

namespace iHTMLChrome {
namespace CEF {

struct CefEnvironment
{
	std::string gameDir;
	std::string cefDir;
	std::string resourcesDir;
	std::string frameworkDir;
	std::string subprocessPath;
	std::string cachePath;
};

bool BuildCefEnvironment( CefEnvironment& env );

} // namespace CEF
} // namespace iHTMLChrome

#endif
