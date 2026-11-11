#pragma once

using ChromeOnPaintCallback =
	void ( * )( void* userdata, const unsigned char* rgba, int width, int height );

namespace Chrome {

bool Initialize();
void Shutdown();
void RunFrame();
void PresentMotd(); // draws the MOTD window (called at the end of the HUD drawing)

int Create( const char* url, int width, int height, ChromeOnPaintCallback callback, void* userdata );
void Destroy( int instance_id );
void Resize( int instance_id, int width, int height );
void Navigate( int instance_id, const char* url );

void SendMouseMove( int instance_id, int x, int y );
void SendMouseButton( int instance_id, int button, bool down );
void SendMouseWheel( int instance_id, int delta_x, int delta_y );
void SendKeyEvent( int instance_id, int key_code, bool down );

unsigned int GetTextureId( int instance_id );

// Server MOTD shown in the game by the embedded browser
bool IsAvailable();                // CEF is built in and did not fail to start (it is started on demand)
bool ShowMotd( const char* url );  // false if CEF is unavailable (the caller uses a fallback)
void HideMotd();
bool IsMotdShown();
bool HandleMotdKey( int down, int keynum ); // true if the key was used by the MOTD window and must not reach the game
bool MotdCapturesMouse();                   // the MOTD window with mouse input is shown: the mouse must not turn the view
void MotdMouseDelta( float dx, float dy );  // mouse movement in pixels
void HideMotdOnLevelChange();               // a new level was loaded: a window left from the previous one is closed
} // namespace Chrome
