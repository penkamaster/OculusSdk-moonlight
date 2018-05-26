/************************************************************************************

Filename    :   GuiSys.h
Content     :   Manager for native GUIs.
Created     :   June 6, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_OvrGuiSys_h )
#define OVR_OvrGuiSys_h

#include "VRMenuObject.h"
#include "OVR_Input.h"

namespace OVR {

class OvrGuiSys;
class OvrVRMenuMgr;
class VRMenuEvent;
class ovrFrameInput;
class App;
class VRMenu;
class OvrGazeCursor;
class VrAppInterface;
class ovrTextureManager;
class ovrReflection;

class ovrGuiFrameResult
{
public:
	HitTestResult	HitResult;
};

//==============================================================
// OvrGuiSys
class OvrGuiSys 
{
public:
	friend class VRMenu;

    static char const *		APP_MENU_NAME;
	static Vector4f const	BUTTON_DEFAULT_TEXT_COLOR;
	static Vector4f const	BUTTON_HILIGHT_TEXT_COLOR;

	class SoundEffectPlayer
	{
	public:
		virtual			~SoundEffectPlayer() { }
		virtual bool	Has( const char * name ) const = 0;
		virtual void	Play( const char * name ) = 0;
		virtual void	Stop( const char * name ) { OVR_UNUSED( name ); }
		virtual void	LoadSoundAsset( const char * name ) { OVR_UNUSED( name ); }
	};

	class ovrDummySoundEffectPlayer : public SoundEffectPlayer
	{
	public:
		virtual bool	Has( const char* name ) const;
		virtual void	Play( const char* name );
		virtual void	Stop( const char* name );
		virtual void	LoadSoundAsset( const char* name );
	};


	virtual					~OvrGuiSys() { }

    static OvrGuiSys *		Create();
    static void				Destroy( OvrGuiSys * & guiSys );

	virtual void			Init( App * app, SoundEffectPlayer & soundEffectPlayer,
							        char const * fontName, OvrDebugLines * debugLines ) = 0;
	// Init with a custom font surface for larger-than-normal amounts of text.
	virtual void			Init( App * app_, SoundEffectPlayer & soundEffectPlayer,
							        char const * fontName, BitmapFontSurface * fontSurface,
							        OvrDebugLines * debugLines ) = 0;

	virtual void			Shutdown() = 0;

	virtual void			Frame( ovrFrameInput const & vrFrame,
									Matrix4f const & centerViewMatrix ) = 0;

	virtual void			Frame( ovrFrameInput const & vrFrame,
									Matrix4f const & centerViewMatrix, Matrix4f const & traceMat ) = 0;

	virtual void			AppendSurfaceList( Matrix4f const & centerViewMatrix, Array< ovrDrawSurface > * surfaceList ) const = 0;

	// called when the app menu is up and a key event is received. Return true if the menu consumed
	// the event.
	virtual bool			OnKeyEvent( int const keyCode, const int repeatCount, KeyEventType const eventType ) = 0;

	virtual void			ResetMenuOrientations( Matrix4f const & viewMatrix ) = 0;

	virtual HitTestResult	TestRayIntersection( const Vector3f & start, const Vector3f & dir ) const = 0;

	//-------------------------------------------------------------
	// Menu management
	
	// Add a new menu that can be opened to receive events
	virtual void			AddMenu( VRMenu * menu ) = 0;
	// Removes and frees a menu that was previously added
	virtual void			DestroyMenu( VRMenu * menu ) = 0;

	// Return the menu with the matching name
	virtual VRMenu *		GetMenu( char const * menuName ) const = 0;

	// Return a list of all the menu names
	virtual Array< String > GetAllMenuNames() const = 0;

	// Opens a menu and places it in the active list
	virtual void			OpenMenu( char const * name ) = 0; 

	// Closes a menu. It will be removed from the active list once it has finished closing.
	virtual void			CloseMenu( const char * name, bool const closeInstantly ) = 0;
	// Closes a menu. It will be removed from the active list once it has finished closing.
	virtual void			CloseMenu( VRMenu * menu, bool const closeInstantly ) = 0;
		
	virtual bool			IsMenuActive( char const * menuName ) const = 0;
	virtual bool			IsAnyMenuActive() const = 0;
	virtual bool			IsAnyMenuOpen() const = 0;

	virtual	void			ShowInfoText( float const duration, const char * fmt, ... ) = 0;
	virtual	void			ShowInfoText( float const duration, Vector3f const & offset, Vector4f const & color, const char * fmt, ... ) = 0;

	//----------------------------------------------------------
	// interfaces

	// FIXME: we should eventually be able to remove the App *
	virtual App *					GetApp() const = 0;
	virtual OvrVRMenuMgr &			GetVRMenuMgr() = 0;
	virtual OvrVRMenuMgr const &	GetVRMenuMgr() const = 0;
	virtual OvrGazeCursor &			GetGazeCursor() = 0;
	virtual BitmapFont &			GetDefaultFont() = 0;
	virtual BitmapFont const &		GetDefaultFont() const = 0;
	virtual BitmapFontSurface &		GetDefaultFontSurface() = 0;
	virtual OvrDebugLines &			GetDebugLines() = 0;
    virtual SoundEffectPlayer &     GetSoundEffectPlayer() = 0;
	virtual ovrTextureManager &		GetTextureManager() = 0;
	virtual ovrReflection &			GetReflection() = 0;
	virtual ovrReflection const &	GetReflection() const = 0;


private:
	//----------------------------------------------------------
	// private methods for VRMenu
	virtual void					MakeActive( VRMenu * menu ) = 0;
};

} // namespace OVR

#endif // OVR_OvrGuiSys_h
