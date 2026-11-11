#include "chrome_backend.h"

#if defined(CS16CLIENT_ENABLE_CEF)

#include "chrome_debug.h"
#include <cmath>
#include <cstring>
#include <string>

#include "chrome_instance.h"

#include "cef_browser.h"
#include "cef_client.h"
#include "cef_parser.h"
#include "cef_render_handler.h"
#include "cef_request.h"
#include "cef_request_handler.h"

#include "chrome_cef_input.h"

namespace iHTMLChrome {
namespace CEF {

static std::string NormalizeLoadUrlForCEF( const std::string& url )
{
	static const char kDataHtmlBase64Prefix[] = "data:text/html;base64,";
	static const char kDataHtmlPrefix[] = "data:text/html,";
	if( url.compare( 0, strlen( kDataHtmlBase64Prefix ), kDataHtmlBase64Prefix ) == 0 )
		return url;
	if( url.compare( 0, strlen( kDataHtmlPrefix ), kDataHtmlPrefix ) == 0 )
	{
		const std::string html = url.substr( strlen( kDataHtmlPrefix ) );
		const std::string html_base64 = CefBase64Encode( html.data(), html.size() ).ToString();
		return std::string( "data:text/html;base64," ) + html_base64;
	}
	return url;
}

namespace
{
// Chromium/CEF: zoom factor ≈ 1.2^level; level 0 => 100%.
constexpr double kDefaultPageScale = 1.0;
double g_browser_page_scale = kDefaultPageScale;
int g_chrome_windowless_target_fps = 60;
int g_live_browsers = 0; // OnAfterCreated .. OnBeforeClose

double PageScaleToCEFZoomLevel( double scale )
{
	if( scale <= 0.001 )
		scale = 0.001;
	double level = std::log( scale ) / std::log( 1.2 );
	if( level < -10.0 )
		level = -10.0;
	if( level > 10.0 )
		level = 10.0;
	return level;
}

// Only web pages can be opened by the pages: no file://, chrome://, devtools:// and so on
static bool IsAllowedNavigationUrl( const std::string& url )
{
	std::string lower( url );
	for( char& c : lower )
		if( c >= 'A' && c <= 'Z' )
			c = static_cast<char>( c - 'A' + 'a' );

	auto starts = [&lower]( const char* prefix ) { return lower.compare( 0, strlen( prefix ), prefix ) == 0; };

	return starts( "http://" ) || starts( "https://" ) || starts( "about:blank" ) || starts( "data:text/html" );
}

class CEFRenderClient final : public CefClient, public CefLifeSpanHandler, public CefRenderHandler, public CefLoadHandler, public CefRequestHandler
{
public:
	explicit CEFRenderClient( ChromeInstance* instance ) : instance_( instance )
	{
	}

	void DetachInstance()
	{
		instance_ = nullptr;
	}

	void SetViewSize( int width, int height )
	{
		width_ = width > 0 ? width : 1;
		height_ = height > 0 ? height : 1;
	}

	void LoadURL( const std::string& url )
	{
		pending_url_ = url;
		if( browser_ && browser_->GetMainFrame() )
			browser_->GetMainFrame()->LoadURL( NormalizeLoadUrlForCEF( pending_url_ ) );
	}

	void Resize()
	{
		if( browser_ && browser_->GetHost() )
		{
			browser_->GetHost()->WasResized();
			browser_->GetHost()->Invalidate( PET_VIEW );
		}
	}

	void Close()
	{
		if( browser_ && browser_->GetHost() )
			browser_->GetHost()->CloseBrowser( true );
	}

	void SendInput( const InputEvent& event )
	{
		if( !browser_ || !browser_->GetHost() )
			return;

		CefRefPtr<CefBrowserHost> host = browser_->GetHost();
		DispatchInputToCef( host, event );
	}

	void ApplyZoomLevel()
	{
		if( !browser_ || !browser_->GetHost() )
			return;
		if( std::abs( last_applied_page_scale_ - g_browser_page_scale ) < 1e-6 )
			return;
		last_applied_page_scale_ = g_browser_page_scale;
		const double want = PageScaleToCEFZoomLevel( g_browser_page_scale );
		applied_cef_zoom_valid_ = true;
		applied_cef_zoom_level_ = want;
		CefRefPtr<CefBrowserHost> host = browser_->GetHost();
		host->SetZoomLevel( want );
		host->WasResized();
		host->Invalidate( PET_VIEW );
	}

	void ApplyFrameRate()
	{
		if( !browser_ || !browser_->GetHost() )
			return;
		const int want = g_chrome_windowless_target_fps;
		if( applied_windowless_fps_ == want )
			return;
		applied_windowless_fps_ = want;
		browser_->GetHost()->SetWindowlessFrameRate( want );
	}

	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
	CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
	CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
	CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }

	bool OnBeforeBrowse( CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest> request, bool, bool ) override
	{
		if( !request )
			return false;

		const std::string url = request->GetURL().ToString();
		if( IsAllowedNavigationUrl( url ))
			return false;

		Chrome_Printf( "[chrome] navigation blocked: %s\n", url.c_str() );
		return true; // cancel
	}

	void GetViewRect( CefRefPtr<CefBrowser>, CefRect& rect ) override
	{
		rect = CefRect( 0, 0, width_, height_ );
	}

	void OnPaint( CefRefPtr<CefBrowser>, PaintElementType type, const RectList&, const void* buffer, int width, int height ) override
	{
		if( type != PET_VIEW || !instance_ || !buffer || width <= 0 || height <= 0 )
			return;

		++frame_sequence_;
		instance_->OnCefPaint( static_cast<const std::uint8_t*>( buffer ), width, height, frame_sequence_ );
	}

	void OnAcceleratedPaint( CefRefPtr<CefBrowser>, PaintElementType type, const RectList&, const CefAcceleratedPaintInfo& ) override
	{
		if( type != PET_VIEW )
			return;
	}

	void OnAfterCreated( CefRefPtr<CefBrowser> browser ) override
	{
		++g_live_browsers;
		browser_ = browser;
		applied_cef_zoom_valid_ = false;
		applied_windowless_fps_ = -1;
		last_applied_page_scale_ = -1.0;
		if( browser_ && browser_->GetHost() )
		{
			CefRefPtr<CefBrowserHost> host = browser_->GetHost();
			const double z = PageScaleToCEFZoomLevel( g_browser_page_scale );
			last_applied_page_scale_ = g_browser_page_scale;
			host->SetZoomLevel( z );
			applied_cef_zoom_valid_ = true;
			applied_cef_zoom_level_ = z;
			host->WasResized();
			host->Invalidate( PET_VIEW );
			host->SetFocus( true );
		}
		if( !pending_url_.empty() && browser_ && browser_->GetMainFrame() )
		{
			LoadURL( pending_url_ );
		}
		if( browser_ && browser_->GetHost() )
		{
			browser_->GetHost()->WasResized();
			browser_->GetHost()->Invalidate( PET_VIEW );
		}
	}

	void OnBeforeClose( CefRefPtr<CefBrowser> ) override
	{
		if( g_live_browsers > 0 )
			--g_live_browsers;
		browser_ = nullptr;
	}

	bool OnBeforePopup( CefRefPtr<CefBrowser>,
		CefRefPtr<CefFrame> frame,
		int,
		const CefString& target_url,
		const CefString&,
		CefLifeSpanHandler::WindowOpenDisposition,
		bool,
		const CefPopupFeatures&,
		CefWindowInfo&,
		CefRefPtr<CefClient>&,
		CefBrowserSettings&,
		CefRefPtr<CefDictionaryValue>&,
		bool* ) override
	{
		if( frame )
			frame->LoadURL( target_url );
		else if( browser_ && browser_->GetMainFrame() )
			browser_->GetMainFrame()->LoadURL( target_url );
		return true; // Block native popup window creation.
	}

	void OnLoadingStateChange( CefRefPtr<CefBrowser>, bool, bool, bool ) override {}

	void OnLoadEnd( CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int ) override
	{
		// Chromium resets page zoom on navigation; re-apply cl_chrome_zoom (g_browser_page_scale).
		if( !frame || !frame->IsMain() || !browser_ )
			return;
		CefRefPtr<CefBrowserHost> host = browser_->GetHost();
		if( !host )
			return;
		const double z = PageScaleToCEFZoomLevel( g_browser_page_scale );
		last_applied_page_scale_ = g_browser_page_scale;
		host->SetZoomLevel( z );
		applied_cef_zoom_valid_ = true;
		applied_cef_zoom_level_ = z;
		host->WasResized();
		host->Invalidate( PET_VIEW );
	}

	void OnLoadError( CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code, const CefString& error_text, const CefString& failed_url ) override
	{
		if( !frame || !frame->IsMain() )
			return;
		if( static_cast<int>( code ) == -3 )
			return;
		Chrome_DPrintf( 1.f, "[browser] CEF load: main frame load error code=%d text='%s' url='%s'\n",
			static_cast<int>( code ), error_text.ToString().c_str(), failed_url.ToString().c_str() );
	}

private:
	ChromeInstance* instance_ = nullptr;
	CefRefPtr<CefBrowser> browser_;
	std::string pending_url_;
	int width_ = 1;
	int height_ = 1;
	std::uint64_t frame_sequence_ = 0;
	bool applied_cef_zoom_valid_ = false;
	double applied_cef_zoom_level_ = 0.0;
	double last_applied_page_scale_ = -1.0;
	int applied_windowless_fps_ = -1;

	IMPLEMENT_REFCOUNTING( CEFRenderClient );
};
} // namespace

class CEFBackend final : public ChromeBackend
{
public:
	bool Initialize( ChromeInstance& instance, const char* start_url ) override
	{
		instance_ = &instance;
		client_ = new CEFRenderClient( instance_ );

		CefWindowInfo window_info;
#if defined(XASH_WIN32)
		window_info.SetAsWindowless( nullptr );
#else
		window_info.SetAsWindowless( 0 );
		window_info.shared_texture_enabled = 0;
#endif

		CefBrowserSettings settings;
		settings.windowless_frame_rate = g_chrome_windowless_target_fps;
		settings.background_color = CefColorSetARGB( 255, 255, 255, 255 );
		client_->SetViewSize( width_, height_ );

		std::string initial = "about:blank";
		if( start_url && start_url[0] )
			initial = NormalizeLoadUrlForCEF( std::string( start_url ) );

		CefRefPtr<CefBrowser> browser = CefBrowserHost::CreateBrowserSync(
			window_info, client_, initial, settings, nullptr, nullptr );

		if( browser.get() == nullptr )
		{
			client_ = nullptr;
			return false;
		}

		if( browser->GetHost() )
		{
			browser->GetHost()->WasResized();
			browser->GetHost()->Invalidate( PET_VIEW );
		}

		return true;
	}

	void Shutdown() override
	{
		if( client_ )
		{
			client_->DetachInstance();
			client_->Close();
			client_ = nullptr;
		}
		instance_ = nullptr;
	}

	void LoadURL( const std::string& url ) override
	{
		if( client_ )
		{
			client_->LoadURL( url );
			client_->Resize();
		}
	}

	void Resize( int width, int height ) override
	{
		width_ = width;
		height_ = height;
		if( client_ )
		{
			client_->SetViewSize( width_, height_ );
			client_->Resize();
		}
	}

	void HandleInput( const InputEvent& event ) override
	{
		if( client_ )
			client_->SendInput( event );
	}

	void Tick() override
	{
		if( client_ )
		{
			client_->ApplyZoomLevel();
			client_->ApplyFrameRate();
		}
	}

private:
	ChromeInstance* instance_ = nullptr;
	CefRefPtr<CEFRenderClient> client_;
	int width_ = 0;
	int height_ = 0;
};

std::unique_ptr<ChromeBackend> CreateChromeBackend()
{
	return std::make_unique<CEFBackend>();
}

void SetChromeZoom( double scale )
{
	if( scale < 0.05 )
		scale = 0.05;
	if( scale > 5.0 )
		scale = 5.0;
	g_browser_page_scale = scale;
}

void SetChromeFrameRate( int frames_per_second )
{
	if( frames_per_second < 1 )
		frames_per_second = 1;
	if( frames_per_second > 240 )
		frames_per_second = 240;
	g_chrome_windowless_target_fps = frames_per_second;
}

int LiveBrowserCount()
{
	return g_live_browsers;
}
} // namespace CEF
} // namespace iHTMLChrome
#endif
