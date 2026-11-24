//
// scoreboard_layout.h
//
// The table of the players (Tab) as in the Steam version of CS 1.6, measured on its screenshot at 1920x1080.
// resource/UI/ScoreBoard.res of the Steam version: the window is 520 x 340 units (1/480 of the screen height,
// 2.25 pixels at 1080) in the middle of the screen; ClientScheme.res: the background is "ListBG" (0 0 0 128),
// the texts are in the font "Default" (Verdana, weight 600), the headers in "BaseText" (255 176 0), the line
// under them in "BorderDark" (188 112 0 128).
//
// At 1920x1080 (pixels):
//   window 375..1544 x 157..921: the background covers 376..1543 x 158..920 (corners of 8), the left column
//   and the top row of it are half covered
//   the name of the server: capitals 165..175 from x 388; the headers: capitals 169..179, right aligned
//   the line under the headers: y 185, x 381..1537
//   a team: capitals 202..212 from x 436, its line (the colour of the team, 1 pixel) at 218
//   the players: rows of 47 pixels from 220, capitals 18 pixels below the top of a row, the row of the local
//   player is lit (white, 10 %) at 380..1539
//   the right edges of the columns: HP 1073, Money 1201, Score 1302, Deaths 1405, Latency 1517
//   the status ("Dead", "Bomb", "D. Kit", "VIP") is left aligned: at 855, or at 1074 if there are no columns of
//   HP and Money (a spectator doesn't have them)
//   many players (31, at 1080): rows of 24 pixels instead of 47, the window grows to fit them and stays in the
//   middle of the screen (117..963 for 2 teams); the next team: its capitals 15 pixels below the last row
//
// No dependencies on the engine, so it can be tested alone.
//

#pragma once

namespace SbLayout {

enum Column
{
	COL_HP = 0,
	COL_MONEY,
	COL_SCORE,
	COL_DEATHS,
	COL_PING,
	NUM_COLUMNS
};

struct Layout
{
	float u = 1.0f;                       // pixels per unit
	int panelX = 0, panelY = 0, panelW = 0, panelH = 0; // the whole window
	int bgX = 0, bgY = 0, bgW = 0, bgH = 0;             // the part that is fully covered
	int radius = 0;

	int titleX = 0, titleCapTop = 0, titleMaxW = 0;
	int headCapTop = 0;

	int lineX = 0, lineW = 0;             // the lines under the headers and the teams
	int headLineY = 0;

	int nameX = 0;                        // the names of the teams and the players
	int colRight[NUM_COLUMNS] = {};       // right edges of the columns
	int statusX = 0;                      // "Dead", "Bomb", "D. Kit", "VIP": left aligned before the HP
	int statusXNoMoney = 0;               // the same if there are no columns of HP and Money

	int litX = 0, litW = 0;               // the light of the row of the local player

	// a section (a team): from its top
	int teamCapTop = 0;                   // the capitals of the name of the team
	int teamLineY = 0;                    // its line
	int teamRows = 0;                     // the first row of its players

	int rowH = 0;                         // a row of a player
	int rowCapTop = 0;                    // the capitals in a row, from its top

	int bottom = 0;                       // nothing is drawn below it
};

inline Layout Compute( int screenW, int screenH )
{
	Layout l;

	// on a screen narrower than 520 units the window is made smaller to fit
	float u = screenH / 480.0f;
	if( 520.0f * u > screenW )
		u = screenW / 520.0f;
	l.u = u;

	auto U = [u]( float v ) { return int( v * u + 0.5f ); };

	l.panelX = int( screenW * 0.5f - 260.0f * u );
	l.panelY = int( screenH * 0.5f - 170.0f * u );
	if( l.panelX < 0 ) l.panelX = 0;
	if( l.panelY < 0 ) l.panelY = 0;
	l.panelW = U( 520.0f );
	l.panelH = U( 340.0f );

	l.bgX = l.panelX + 1;
	l.bgY = l.panelY + 1;
	l.bgW = l.panelW - 2;
	l.bgH = l.panelH - 2;
	l.radius = U( 3.6f );

	l.titleX = l.panelX + U( 5.33f );
	l.titleCapTop = l.panelY + U( 3.56f );
	l.titleMaxW = U( 250.0f );            // the label of ScoreBoard.res is 250 units wide
	l.headCapTop = l.panelY + U( 5.33f );

	l.lineX = l.panelX + U( 2.67f );
	l.lineW = l.panelX + U( 516.9f ) - l.lineX;
	l.headLineY = l.panelY + U( 12.44f );

	l.nameX = l.panelX + U( 26.67f );

	static const float colRight[NUM_COLUMNS] = { 310.2f, 367.1f, 412.0f, 457.8f, 507.6f };
	for( int i = 0; i < NUM_COLUMNS; ++i )
		l.colRight[i] = l.panelX + U( colRight[i] );

	l.statusX = l.panelX + U( 213.3f );
	l.statusXNoMoney = l.panelX + U( 310.7f );

	l.litX = l.panelX + U( 2.22f );
	l.litW = l.panelX + U( 517.8f ) - l.litX;

	l.teamCapTop = U( 7.56f );
	l.teamLineY = U( 14.67f );
	l.teamRows = U( 15.56f );

	l.rowH = U( 20.89f );
	l.rowCapTop = U( 8.0f );

	l.bottom = l.bgY + l.bgH - U( 2.0f );
	return l;
}

// The low rows of the Steam version for many players (24 pixels at 1080)
inline int SmallRowHeight( float u )
{
	return int( 10.67f * u + 0.5f );
}

// The height of the rows and the window for all the players: sections - the teams with a header (0 if the
// players are listed without teams), players - all the rows, capHeight - the height of the capitals of the font
// (pixels of the layout). Like the Steam version: the rows are high (47 pixels at 1080) if all of them fit,
// else low (24); the window grows to fit them up to almost the whole height of the screen and stays in the
// middle of it. If even the low rows don't fit, they are made lower, down to the height of the text.
// The text stays in the middle of its row.
inline void Fit( Layout &l, int screenH, int sections, int players, int capHeight )
{
	const float u = l.u;
	auto U = [u]( float v ) { return int( v * u + 0.5f ); };

	if( players < 1 )
		players = 1;
	if( sections < 0 )
		sections = 0;

	// the parts that don't depend on the rows: the headers, the headers of the teams (the next team starts
	// 2 pixels higher than the end of the rows above it), the space at the bottom
	const int top = l.headLineY - l.panelY;
	const int headers = sections > 0 ? sections * l.teamRows - ( sections - 1 ) * 2 : l.teamRows;
	const int fixed = top + headers + U( 3.0f );

	// the highest window: 10 units from the top and the bottom of the screen
	const int maxH = screenH - U( 10.0f ) * 2;

	int rowH = l.rowH;

	if( fixed + players * rowH > maxH )
	{
		rowH = SmallRowHeight( u );

		if( fixed + players * rowH > maxH )
		{
			// even the low rows don't fit: lower, but not lower than the text with a little space
			const int minRow = capHeight * 2 > 4 ? capHeight * 2 : 4;
			rowH = ( maxH - fixed ) / players;
			if( rowH < minRow )
				rowH = minRow;
		}
	}

	int panelH = fixed + players * rowH;

	if( panelH < l.panelH )
		panelH = l.panelH;  // never smaller than in ScoreBoard.res
	if( panelH > maxH && maxH >= l.panelH )
		panelH = maxH;      // the rows that don't fit are not drawn

	const int dy = ( panelH - l.panelH ) / 2; // it stays in the middle of the screen
	if( dy > 0 )
	{
		int newY = l.panelY - dy;
		if( newY < 0 )
			newY = 0;
		const int shift = newY - l.panelY;

		l.panelY += shift;
		l.bgY += shift;
		l.titleCapTop += shift;
		l.headCapTop += shift;
		l.headLineY += shift;
	}

	l.panelH = panelH;
	l.bgH = panelH - 2;
	l.bottom = l.bgY + l.bgH - U( 1.0f );

	// the text in the middle of the row (as in the Steam version: 18 pixels in a row of 47 at 1080, 7 in 24)
	if( rowH != l.rowH )
	{
		const int capTop = ( rowH - capHeight + 1 ) / 2;
		l.rowCapTop = capTop > 0 ? capTop : 0;
		l.rowH = rowH;
	}
}

} // namespace SbLayout
