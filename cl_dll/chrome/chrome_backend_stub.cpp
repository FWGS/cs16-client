#include "chrome_backend.h"

namespace iHTMLChrome {
namespace CEF {
class NullChromeBackend final : public ChromeBackend
{
public:
	bool Initialize( ChromeInstance&, const char* ) override { return true; }
	void Shutdown() override {}
	void LoadURL( const std::string& ) override {}
	void Resize( int, int ) override {}
	void HandleInput( const InputEvent& ) override {}
	void Tick() override {}
};

#if !defined(CS16CLIENT_ENABLE_CEF)
std::unique_ptr<ChromeBackend> CreateChromeBackend()
{
	return std::make_unique<NullChromeBackend>();
}

void SetChromeZoom( double scale )
{
	(void)scale;
}

void SetChromeFrameRate( int frames_per_second )
{
	(void)frames_per_second;
}

int LiveBrowserCount()
{
	return 0;
}
#endif
} // namespace CEF
} // namespace iHTMLChrome
