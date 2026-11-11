#pragma once

namespace iHTMLChrome {
namespace CEF {
class CefRuntime
{
public:
	static CefRuntime& Get();

	bool Initialize();
	void DoMessageLoopWork();
	void Shutdown();

	bool initialized() const { return initialized_; }

private:
	bool initialized_ = false;
};
} // namespace CEF
} // namespace iHTMLChrome
