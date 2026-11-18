//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_STATUS_HUD_H
#define VOICE_STATUS_HUD_H
#pragma once

#include <utlvector.h>
// #include <vgui_controls/Panel.h>
// #include <vgui_controls/Label.h>
// #include <vgui_controls/ImagePanel.h>
// #include <color.h>
// #include <vgui/IScheme.h>
// #include <vgui_controls/Controls.h>

#ifdef CZERO
#include "vgui/hl_base/iviewport.h"
#else
// #include <game_controls/iviewport.h>
#endif

#include "voice_common.h"
#include "cl_entity.h"
#include "voice_banmgr.h"
#include "draw_util.h"
#include "hud/spectator_layout.h"
#include "hud/text_utf8.h"
#include "triangleapi.h"

extern int g_VoiceLabelIcon;

//-----------------------------------------------------------------------------
// Purpose: a label which displays a name and a speaking icon
//-----------------------------------------------------------------------------
class CVoiceLabel
{
public:
	CVoiceLabel()
	{
		// m_pLabel = new VoiceVGUILabel( /*NULL,*/ "VoiceLabel", "" );
		// m_pLabel->SetParent( gViewPortInterface->GetViewPortPanel() );
		// m_pLabel->SetProportional(true);
		// m_pLabel->SetScheme("ClientScheme");
		// vgui::SETUP_PANEL(m_pLabel);
		m_clientindex = -1; // -1 means unassigned
		m_locationString = NULL;
		m_playerName = NULL;
	}

	~CVoiceLabel()
	{
		// m_pLabel->MarkForDeletion();
	}

	// pass throughs for various label calls
	void SetFgColor( RGBA c ) { m_fgColor = c; }
	void SetBgColor( RGBA c ) { m_bgColor = c; }
	void SetVisible( bool state ) { m_visible = state; }
	bool GetVisible() { return m_visible; }

	// the name in "Default" of the Steam version (Verdana, weight 600) from the atlas of the font: sharp, one texel
	// on one pixel (the font of the HUD was scaled and blurred); false without the fonts of Windows
	bool UpdateText()
	{
		if( !DrawUtils::UseAtlasText() )
			return false;

		// the spaces after the name (RebuildLabelText) were the right margin of the label: it is set here
		std::string name = TextUtf::ToUtf8( m_buf );
		while( !name.empty() && name.back() == ' ' )
			name.pop_back();

		const SpecBar::Font font = SpecBar::FontFor( TrueHeight );
		return m_text.Update( name, "Verdana", SpecBar::FontEm( font.tall ), 600, font.antialias )
			&& m_text.Valid();
	}

	// As the label of the Steam version (measured at 1920x1080): as high as the font (18 pixels at 1080), the
	// speaker (gfx/vgui/speaker4.tga, 16 x 16) at its left edge in its own size, the name 2 pixels after it, 4
	// pixels of margin on the right; everything in whole pixels of the screen
	enum { ICON_GAP = 2, RIGHT_MARGIN = 4 };

	static int IconSize()
	{
		int size = 16;
		if( g_VoiceLabelIcon > 0 && gRenderAPI.RenderGetParm )
		{
			const int w = gRenderAPI.RenderGetParm( PARM_TEX_WIDTH, g_VoiceLabelIcon );
			if( w > 0 && w <= 64 )
				size = w;
		}
		return size;
	}

	void GetContentSize( int &wide, int &tall )
	{
		// m_pLabel->GetContentSize( wide, tall );
		const float scale = gHUD.m_flScale > 0.0f ? gHUD.m_flScale : 1.0f;
		if( UpdateText() )
		{
			const int icon = IconSize();
			int tallPx = m_text.Height();
			if( tallPx < icon + 2 )
				tallPx = icon + 2;
			wide = (int)( ( icon + ICON_GAP + m_text.Width() + RIGHT_MARGIN ) / scale + 0.999f );
			tall = (int)( tallPx / scale + 0.999f );
			return;
		}

		wide = DrawUtils::HudStringLen( m_buf );

		tall = gHUD.GetCharHeight();

		if ( tall < 32 )
			tall = 32;

		wide += tall - 2;
	}

	void SetBounds( int x, int y )
	{
		// m_pLabel->SetPos( x, y );
		this->x = x;
		this->y = y;
		// int wide, tall;
		// m_pLabel->GetContentSize( wide, tall );
		// m_pLabel->SetSize( wide, tall );
		GetContentSize( wide, tall );
	}

	void Draw()
	{
		if ( !GetVisible() )
			return;

		int offset = 1;
		int iconsize = tall - offset * 2;

		gEngfuncs.pfnFillRGBABlend( x, y, wide, tall, m_bgColor.r, m_bgColor.g, m_bgColor.b, m_bgColor.a );

		if( UpdateText() )
		{
			const float scale = gHUD.m_flScale > 0.0f ? gHUD.m_flScale : 1.0f;
			const int px = (int)( x * scale + 0.5f ), py = (int)( y * scale + 0.5f );
			const int tallPx = (int)( tall * scale + 0.5f );
			const int icon = IconSize();
			const int iconY = py + ( tallPx - icon ) / 2;

			gRenderAPI.GL_SelectTexture( 0 );
			gRenderAPI.GL_Bind( 0, g_VoiceLabelIcon );
			gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
			gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
			DrawUtils::Draw2DQuad( (float)px, (float)iconY, (float)( px + icon ), (float)( iconY + icon ));

			// the cell of the text in the middle of the label (at its top when they are as high, as in Steam)
			const int top = py + ( tallPx - m_text.Height() ) / 2;
			m_text.Draw( (float)( px + icon + ICON_GAP ), (float)top, m_fgColor.r, m_fgColor.g, m_fgColor.b );
			return;
		}

		gRenderAPI.GL_SelectTexture( 0 );
		gRenderAPI.GL_Bind( 0, g_VoiceLabelIcon );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		DrawUtils::Draw2DQuad( x * gHUD.m_flScale,
		                       ( y + offset ) * gHUD.m_flScale,
		                       ( x + iconsize ) * gHUD.m_flScale,
		                       ( y + iconsize + offset ) * gHUD.m_flScale );

		int textx = x + iconsize + offset;

		int texty = y + ( tall - gHUD.GetCharHeight() ) / 2;
		DrawUtils::DrawHudString( textx, texty, textx + wide, m_buf, m_fgColor.r, m_fgColor.g, m_fgColor.b );
	}

	void SetClientIndex( int in ) { m_clientindex = in; }
	int GetClientIndex() { return m_clientindex; }

	void SetLocation( const char *location );
	void SetPlayerName( const char *name );

private:
	//-----------------------------------------------------------------------------
	// Purpose: inner class that overrides ApplySchemeSettings() for the label so an image can be loaded, also saves colors away
	//   so ApplySchemeSettings() doesn't override them
	//-----------------------------------------------------------------------------
	// class VoiceVGUILabel // : public vgui::Label
	// {
	// public:
	// 	VoiceVGUILabel( /* vgui::Panel *parent,*/ const char *name, const char *text ) /*: Label(parent, name, text)*/ { }

	// private:
	// VGUI2 overrides
	// virtual void ApplySchemeSettings(vgui::IScheme *pScheme)
	// {
	// 	Label::ApplySchemeSettings(pScheme);
	// 	SetTextImageIndex(1);
	// 	SetImagePreOffset( 1, 2); // shift the text over a little
	// 	// you need to load the image here, after Label::ApplySchemeSettings()(as applysettings nulls out all existing images)
	// 	SetImageAtIndex( 0, vgui::scheme()->GetImage( "gfx/vgui/speaker4", false), 1 );
	// }
	// };

	void RebuildLabelText();

	// VoiceVGUILabel *m_pLabel; // the label with the user name and icon
	int m_clientindex;      // Client index of the speaker. -1 if this label isn't being used.
	char *m_locationString; // localized location string.  NULL if the location is "".
	char *m_playerName;

	RGBA m_fgColor;
	RGBA m_bgColor;
	bool m_visible;

	int x, y, wide, tall;
	CTextTexture m_text; // the name (see UpdateText)
	char m_buf[512];
	int iconwidth;
};

//-----------------------------------------------------------------------------
// Purpose: Handles the displaying of labels on the hud and icons above players in game when they talk
//-----------------------------------------------------------------------------
class CVoiceStatusHud : public IVoiceHud, public CHudBase
{
public:
	CVoiceStatusHud();
	virtual ~CVoiceStatusHud();

	// CHudBase overrides.
	// Initialize the cl_dll's voice manager.
	virtual int Init( IVoiceStatusHelper *pHelper, IVoiceStatus *pStatus );

	// ackPosition is the bottom position of where CVoiceStatus will draw the voice acknowledgement labels.
	virtual int VidInit();

	// Call from the HUD_CreateEntities function so it can add sprites above player heads.
	void CreateEntities();

	void UpdateLocation( int entindex, const char *location );

	void UpdateSpeakerStatus( int entindex, bool bTalking );

	CVoiceLabel *FindVoiceLabel( int clientindex ); // Find a CVoiceLabel representing the specified speaker.
	                                                // Returns NULL if none.
	                                                // entindex can be -1 if you want a currently-unused voice label.
	CVoiceLabel *GetFreeVoiceLabel();               // Get an unused voice label. Returns NULL if none.
	void RepositionLabels();

	void Shutdown( void );
	int Draw( float flTime );

private:
	cl_entity_s m_VoiceHeadModels[VOICE_MAX_PLAYERS]; // These aren't necessarily in the order of players. They are just
	                                                  // a place for it to put data in during CreateEntities.
	HSPRITE m_VoiceHeadModel;                         // Voice head model (goes above players who are speaking).
	float m_VoiceHeadModelHeight;                     // Height above their head to place the model.

	IVoiceStatusHelper *m_pHelper;
	IVoiceStatus *m_pStatus;

	// Labels telling who is speaking.
	CUtlVector<CVoiceLabel *> m_Labels;

	// vgui::ImagePanel *m_pLocalPlayerTalkIcon;
	int m_pLocalPlayerTalkIcon;

	bool m_LocalPlayerTalkIconVisible;
	int m_LocalPlayerTalkIconXPos, m_LocalPlayerTalkIconYPos;
	int m_LocalPlayerTalkIconXSize, m_LocalPlayerTalkIconYSize;
};

#endif // VOICE_STATUS_HUD_H
