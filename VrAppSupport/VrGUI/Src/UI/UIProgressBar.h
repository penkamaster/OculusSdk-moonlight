/************************************************************************************

Filename    :   UIProgressBar.h
Content     :   Progress bar UI component, with an optional description and cancel button.
Created     :   Mar 11, 2015
Authors     :   Alex Restrepo

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( UIProgressBar_h )
#define UIProgressBar_h

#include "UI/UIObject.h"
#include "UI/UILabel.h"
#include "UI/UIImage.h"
#include "UI/UITexture.h"
#include "UI/UIButton.h"

namespace OVR {

class VrAppInterface;

class UIProgressBar: public UIObject
{
public:
						UIProgressBar( OvrGuiSys &guiSys );

	void 				AddToMenu( UIMenu *menu, bool showDescriptionLabel, bool showCancelButton, UIObject *parent = NULL );
	void 				SetProgress( const float progress );
	void 				SetDescription( const String &description );
	void				SetOnCancel( void ( *callback )( UIButton *, void * ), void *object );
	float				GetProgress() const { return Progress; }
	void				SetProgressImageZOffset( float offset );

private:
	UILabel				DescriptionLabel;

	UITexture			CancelButtonTexture;
	UIButton			CancelButton;

	UITexture			ProgressBarTexture;
	UIImage				ProgressImage;

	UITexture			ProgressBarBackgroundTexture;
	UIImage				ProgressBackground;

	float				Progress;
};

} // namespace OculusCinema

#endif /* UIProgressBar_h */
