/************************************************************************************

Filename    :   UILabel.h
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( UILabel_h )
#define UILabel_h

#include "VRMenu.h"
#include "UI/UIObject.h"

namespace OVR {

class VrAppInterface;

class UILabel : public UIObject
{
public:
										UILabel( OvrGuiSys &guiSys );
										~UILabel();

	void 								AddToMenu( UIMenu *menu, UIObject *parent = NULL );

	void								SetAlign( const HorizontalJustification horiz, const VerticalJustification vert );
	void								SetHorizontalAlign( const HorizontalJustification horiz );
	HorizontalJustification 			GetHorizontalAlign() const;
	void								SetVerticalAlign( const VerticalJustification vert );
	VerticalJustification				GetVerticalAlign() const;

	void								SetText( const char *text );
	void								SetText( const String &text );
	void								SetTextWordWrapped( char const * text, class BitmapFont const & font, float const widthInMeters );
	const String & 						GetText() const;

	void 								CalculateTextDimensions();

	void								SetFontScale( float const scale );
	float 								GetFontScale() const;

	const VRMenuFontParms & 			GetFontParms() const { return FontParms; }
	void								SetFontParms( const VRMenuFontParms &parms );

	void     				           	SetTextOffset( Vector3f const & pos );
    Vector3f const &    				GetTextOffset() const;

	Vector4f const &					GetTextColor() const;
	void								SetTextColor( Vector4f const & c );

	Bounds3f            				GetTextLocalBounds( BitmapFont const & font ) const;

private:
	VRMenuFontParms 					FontParms;

	Vector3f							TextOffset;
	Vector3f							JustificationOffset;

	void 								CalculateTextOffset();
};

} // namespace OVR

#endif // UILabel_h
