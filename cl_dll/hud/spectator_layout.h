//
// spectator_layout.h
//
// The upper bar of the spectator mode as in the Steam version of CS 1.6, measured on its screenshots
// (1920x1080 and 1600x900). Its sizes are in units of 1/480 of the screen height ("proportional" sizes
// of VGUI2) and the elements are placed from the right edge of the screen:
//
//   Counter-Terrorists :  0  |  $800
//          Terrorists :  0  |  (timer) 1:45
//
// No dependencies on the engine, so it can be tested alone.
//

#pragma once

namespace SpecBar {

// One unit in pixels
inline float Unit( int screenH )
{
	return screenH / 480.0f;
}

// Height of the black bars (64 units, 144 pixels at 1080)
inline int BarHeight( int screenH )
{
	return int( screenH * 64 / 480 );
}

// The font of the bar is "Default" of resource/ClientScheme.res (Verdana, weight 600): its size depends
// on the height of the screen; the Steam version draws a "tall" of the scheme with letters of tall * 3 / 4 pixels
// (rounded down): 20 -> 15 at 1080 (capitals of 11 pixels, small letters of 8), 14 -> 10 for the tutor
struct Font
{
	int tall;
	bool antialias;
};

inline Font FontFor( int screenH )
{
	if( screenH < 600 )  return { 12, false };
	if( screenH < 768 )  return { 13, false };
	if( screenH < 1024 ) return { 14, true };
	if( screenH < 1200 ) return { 20, true };
	return { 24, true };
}

inline int FontEm( int tall )
{
	const int em = tall * 3 / 4;
	return em < 4 ? 4 : em;
}

// The top of the capitals of the two rows (38 and 65 of 1080) and their distance from the top of the cell
// of the font (the ascent of Verdana is 1.005 em, its capitals are 0.727 em high)
inline float Row1CapTop( int screenH ) { return 16.9f * Unit( screenH ); }
inline float Row2CapTop( int screenH ) { return 28.9f * Unit( screenH ); }
inline int CellTopFromCapTop( float capTop, int em ) { return int( capTop - 0.278f * em + 0.5f ); }

struct Layout
{
	int teamLabelRight;  // right edge of "Counter-Terrorists :" and "Terrorists :" (right aligned)
	int teamValueX;      // left edge of the scores
	int dividerX, dividerY, dividerW, dividerH; // the vertical line
	int infoX;           // left edge of the money (or the map name) in the first row
	int timerIconX, timerIconY, timerIconSize;
	int timerTextX;      // left edge of the time in the second row
};

inline Layout Compute( int screenW, int screenH )
{
	const float u = Unit( screenH );
	auto fromRight = [screenW, u]( float units ) { return int( screenW - units * u + 0.5f ); };

	Layout l;
	l.teamLabelRight = fromRight( 83.1f );  // ':' ends at 1729 of 1920
	l.teamValueX = fromRight( 80.0f );      // "0" at 1741
	l.dividerX = fromRight( 64.0f );        // 1776..1777
	l.dividerW = int( u + 0.5f ) > 0 ? int( u + 0.5f ) : 1;
	l.dividerY = int( 12.0f * u + 0.5f );   // 27..93
	l.dividerH = int( 30.0f * u + 0.5f ) - 1;
	l.infoX = fromRight( 56.0f );           // "$" at 1795
	l.timerIconX = fromRight( 56.0f );      // the picture of the icon is seen at 1798..1814 x 59..81
	l.timerIconY = int( 25.3f * u + 0.5f );
	l.timerIconSize = int( 12.4f * u + 0.5f );
	l.timerTextX = fromRight( 41.7f );      // 1826: "0:00" is seen at 1827..1865 (1 pixel of bearing)
	return l;
}

} // namespace SpecBar

// The menus of the spectator mode as in the Steam version (measured on its screenshots at 1920x1080):
//   at the bottom: "Options" 11..313, the previous player 567..611, the player 623..1297 (in the middle of the
//   screen), the next player 1308..1352, the camera mode 1605..1907, all of them 1003..1051 (frames of 1 pixel,
//   "BorderBright"); the lists have a small grey arrow on the right, the buttons orange triangles
//   a menu opens above its list (its frame ends 2 pixels above it) and is as wide; its items are 66 pixels high,
//   the text in "DefaultSmall" (Verdana, thin) with its capitals in the middle of the item, 31 pixels from the
//   edge if the menu has check marks (they are at 15), else 7; an open submenu is lit (255 176 0, 48 %)
namespace SpecMenu {

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
};

struct Layout
{
	float u = 1.0f;
	Rect options, prev, player, next, view; // the controls at the bottom
	int itemH = 0;                          // an item of a menu
	int comboTextInset = 0, comboCapTop = 0;
	int menuTextInset = 0, menuCheckTextInset = 0, menuCheckX = 0;

	int U( float v ) const { return int( v * u + 0.5f ); }
};

inline Layout Compute( int screenW, int screenH )
{
	Layout l;
	l.u = screenH / 480.0f;

	const int y = screenH - l.U( 34.2f );
	const int h = l.U( 21.8f );
	const int comboW = l.U( 134.7f );
	const int buttonW = l.U( 20.0f );
	const int mid = screenW / 2;

	l.options = { l.U( 4.9f ), y, comboW, h };
	l.prev    = { mid - l.U( 174.7f ), y, buttonW, h };
	l.player  = { mid - l.U( 149.8f ), y, l.U( 300.0f ), h };
	l.next    = { mid + l.U( 154.7f ), y, buttonW, h };
	l.view    = { screenW - l.U( 140.0f ), y, comboW, h };

	// a narrow screen (4:3, 5:4): the lists on the sides are made narrower to keep the gap of the Steam version
	// (12 pixels at 1080) to the buttons in the middle
	const int gap = l.U( 5.3f );
	if( l.options.x + l.options.w > l.prev.x - gap )
		l.options.w = l.prev.x - gap - l.options.x;
	if( l.view.x < l.next.x + l.next.w + gap )
	{
		const int right = l.view.x + l.view.w;
		l.view.x = l.next.x + l.next.w + gap;
		l.view.w = right - l.view.x;
	}

	l.itemH = l.U( 29.33f );
	l.comboTextInset = l.U( 1.3f );
	l.comboCapTop = l.U( 8.0f );
	l.menuTextInset = l.U( 3.1f );
	l.menuCheckTextInset = l.U( 13.8f );
	l.menuCheckX = l.U( 6.7f );
	return l;
}

// The frame of a menu of "items" items opened above the list "combo"
inline Rect MenuAbove( const Layout &l, const Rect &combo, int items )
{
	Rect r;
	r.x = combo.x;
	r.w = combo.w;
	r.h = items * l.itemH + 2;
	r.y = combo.y - 2 - ( r.h - 1 );
	return r;
}

// The frame of a submenu opened to the right of the item "item" of the menu "menu"; width - its width (as wide
// as its texts in the Steam version: "Settings" 315..501 at 1080), 0 - as wide as the menu
inline Rect SubmenuRight( const Layout &l, const Rect &menu, int item, int items, int width = 0 )
{
	Rect r;
	r.x = menu.x + menu.w;
	r.w = width > 0 ? width : menu.w;
	r.h = items * l.itemH + 2;
	r.y = menu.y + item * l.itemH;
	return r;
}

// The item i of a menu
inline Rect MenuItem( const Layout &l, const Rect &menu, int i )
{
	Rect r;
	r.x = menu.x + 1;
	r.w = menu.w - 2;
	r.y = menu.y + 1 + i * l.itemH;
	r.h = l.itemH;
	return r;
}

// The width of a menu with check marks for its widest text (textW): the text from menuCheckTextInset, 13 pixels
// after it at 1080 (as the "Settings" submenu of the Steam version)
inline int CheckMenuWidth( const Layout &l, int textW )
{
	return l.menuCheckTextInset + textW + l.U( 5.8f );
}

// The list of the players (it opens above the player): as the ComboBox of the Steam version it shows up to
// 10 items at once, the others are reached with a scroll bar on its right side
const int PLAYERS_VISIBLE = 10;

// how many items are seen if there are "count" players (they are not higher than the space above the list)
inline int PlayersVisible( const Layout &l, int count, int screenH, int barH )
{
	int n = count < PLAYERS_VISIBLE ? count : PLAYERS_VISIBLE;
	const int room = ( l.player.y - 2 - barH - 2 ) / ( l.itemH > 0 ? l.itemH : 1 );
	if( n > room )
		n = room;
	return n > 0 ? n : 0;
}

inline int ScrollBarWidth( const Layout &l )
{
	return l.U( 23.1f ); // 52 at 1080: 1244..1295 inside the frame of the list 623..1297
}

inline int ScrollButtonHeight( const Layout &l )
{
	return l.U( 23.6f ); // 53 at 1080: 342..394
}

// the scroll bar inside the frame of the menu; its buttons are squares at its ends
inline Rect ScrollBar( const Layout &l, const Rect &menu )
{
	Rect r;
	r.w = ScrollBarWidth( l );
	r.x = menu.x + menu.w - 1 - r.w;
	r.y = menu.y + 1;
	r.h = menu.h - 2;
	return r;
}

inline Rect ScrollUp( const Layout &l, const Rect &menu )
{
	Rect r = ScrollBar( l, menu );
	r.h = ScrollButtonHeight( l );
	return r;
}

inline Rect ScrollDown( const Layout &l, const Rect &menu )
{
	Rect r = ScrollBar( l, menu );
	r.h = ScrollButtonHeight( l );
	r.y = menu.y + menu.h - 1 - r.h;
	return r;
}

// the thumb of the scroll bar: "first" - the first item seen, "visible" of "count"
inline Rect ScrollThumb( const Layout &l, const Rect &menu, int first, int visible, int count )
{
	// at 1080 with 10 of 15 players: the thumb 396..763 between the buttons 342..394 and 948..1000
	const Rect bar = ScrollBar( l, menu );
	const int bh = ScrollButtonHeight( l );
	Rect r = bar;
	const int track = bar.h - bh * 2;
	if( count <= visible || track <= 0 )
	{
		r.y = bar.y + bh;
		r.h = track > 0 ? track : 0;
		return r;
	}
	int h = track * visible / count;
	const int minH = bh < track ? bh : track;
	if( h < minH )
		h = minH;
	if( h > track )
		h = track;
	const int maxFirst = count - visible;
	r.y = bar.y + bh + ( track - h ) * first / maxFirst;
	r.h = h;
	return r;
}

} // namespace SpecMenu
