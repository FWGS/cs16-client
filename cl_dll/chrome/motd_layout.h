#pragma once

// The window of the server MOTD as in the Steam version of CS 1.6, measured on its screenshot at 1920x1080.
// Like the other VGUI2 panels of the Steam version it is sized in units of 1/480 of the screen height
// (2.25 pixels at 1080) and grows with the resolution:
//
//   panel 1345 x 985 at (285, 45), rounded with the radius 20, a cut of 2 pixels at 117 from its top
//   the CS icon (gfx/vgui/CS_logo.tga, 64 x 64) as a square of 88 at (12, 12), the name of the server (cap height 22) from x 126, its baseline at 70
//   the page 1080 x 540 at (126, 216)
//   the OK button 288 x 45 at (126, 774), "OK" in the font of the scheme, its baseline at 28 of the button
//
// No dependencies on the engine, so it can be tested alone.

namespace MotdWin {

struct Layout
{
	float u = 1.0f;                    // pixels per unit
	int panelX = 0, panelY = 0, panelW = 0, panelH = 0;
	int radius = 0;
	int cutY = 0, cutH = 1;            // the cut: cutY .. cutY + cutH (absolute)
	int iconX = 0, iconY = 0, iconSize = 0;
	int titleX = 0, titleBaseline = 0, titleEm = 0, titleMaxW = 0;
	int pageX = 0, pageY = 0, pageW = 0, pageH = 0;
	int btnX = 0, btnY = 0, btnW = 0, btnH = 0;
	int okCenterX = 0, okBaseline = 0; // "OK": its middle and its baseline
};

// The size of the page (the browser draws it pixel for pixel): 480 x 240 units
inline void PageSize( int screenH, int& w, int& h )
{
	const float u = screenH / 480.0f;
	w = int( 480.0f * u + 0.5f );
	h = int( 240.0f * u + 0.5f );
}

inline Layout Compute( int screenW, int screenH )
{
	Layout l;

	// on a screen narrower than 4:3 x 1.25 the window is made smaller to fit
	float u = screenH / 480.0f;
	if( 600.0f * u > screenW )
		u = screenW / 600.0f;
	l.u = u;

	auto U = [u]( float v ) { return int( v * u + 0.5f ); };

	// the frame of VGUI2 is 600 x 440 units in the middle of the screen; its background ends
	// 2.2 units before its right and bottom edges
	l.panelX = int( screenW * 0.5f - 300.0f * u );
	l.panelY = int( screenH * 0.5f - 220.0f * u );
	if( l.panelX < 0 ) l.panelX = 0;
	if( l.panelY < 0 ) l.panelY = 0;
	l.panelW = U( 597.8f );
	l.panelH = U( 437.8f );
	l.radius = U( 9.0f );

	l.cutY = l.panelY + U( 52.0f );
	l.cutH = int( u ) > 0 ? int( u ) : 1;

	// the figure of CS_logo.tga is 12..53 x 5..56 of 64: seen at 314..368 x 63..135 (a square of 88 at 297, 57)
	l.iconX = l.panelX + U( 5.3f );
	l.iconY = l.panelY + U( 5.3f );
	l.iconSize = U( 39.0f );

	l.titleX = l.panelX + U( 56.0f );
	l.titleBaseline = l.panelY + U( 31.1f );
	l.titleEm = U( 13.8f );
	l.titleMaxW = l.panelW - U( 56.0f ) - U( 20.0f );

	l.pageX = l.panelX + U( 56.0f );
	l.pageY = l.panelY + U( 96.0f );
	PageSize( screenH, l.pageW, l.pageH );
	if( u < screenH / 480.0f )
	{
		l.pageW = U( 480.0f );
		l.pageH = U( 240.0f );
	}

	l.btnX = l.pageX;
	l.btnY = l.panelY + U( 344.0f );
	l.btnW = U( 128.0f );
	l.btnH = U( 20.0f );

	l.okCenterX = l.btnX + U( 67.2f );   // a little right of the middle, as in the Steam version
	l.okBaseline = l.btnY + U( 12.4f );
	return l;
}

} // namespace MotdWin
