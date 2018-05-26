/************************************************************************************

Filename    :   SceneManager.h
Content     :	Handles rendering of current scene and movie.
Created     :   September 3, 2013
Authors     :	Jim Dosé, based on a fork of VrVideo.cpp from VrVideo by John Carmack.

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( SceneManager_h )
#define SceneManager_h

#include "PcManager.h"
#include "SceneView.h"
#include "Lerp.h"

using namespace OVR;

namespace OculusCinema {

class CinemaApp;

class SceneManager
{
public:
						SceneManager( CinemaApp & app_ );

	void 				OneTimeInit( const char * launchIntent );
	void				OneTimeShutdown();

	bool 				Command( const char * msg );
	void 				Frame( const ovrFrameInput & vrFrame );

	void 				SetSeat( int newSeat );
	bool 				ChangeSeats( const ovrFrameInput & vrFrame );
	void                NextSeat();

	void 				ClearMovie();
	void 				PutScreenInFront();

	void				ClearGazeCursorGhosts();  	// clear gaze cursor to avoid seeing it lerp

	void 				ToggleLights( const float duration );
	void 				LightsOn( const float duration );
	void 				LightsOff( const float duration );

	void				SetSceneModel( const SceneDef & sceneDef );
	void				SetSceneProgram( const sceneProgram_t opaqueProgram, const sceneProgram_t additiveProgram );

	Posef				GetScreenPose() const;
	Vector2f			GetScreenSize() const;

	Vector3f			GetFreeScreenScale() const;

	Matrix4f			FreeScreenMatrix() const;
	Matrix4f 			BoundsScreenMatrix( const Bounds3f & bounds, const float movieAspect ) const;
	Matrix4f 			ScreenMatrix() const;

	void				AllowMovement( bool allow ) { AllowMove = allow; }
	bool				MovementAllowed() const { return AllowMove; }

	bool				GetUseOverlay() const;

public:
	CinemaApp &			Cinema;

	// Allow static lighting to be faded up or down
	Lerp				StaticLighting;

	bool				UseOverlay;

	SurfaceTexture	* 	MovieTexture;
	long long			MovieTextureTimestamp;

	// FreeScreen mode allows the screen to be oriented arbitrarily, rather
	// than on a particular surface in the scene.
	bool				FreeScreenActive;
	float				FreeScreenScale;
	float				FreeScreenDistance;
	Matrix4f			FreeScreenPose;

	// don't make these bool, or sscanf %i will trash adjacent memory!
	int					ForceMono;			// only show the left eye of 3D movies

	// Set when MediaPlayer knows what the stream size is.
	// current is the aspect size, texture may be twice as wide or high for 3D content.
	int					CurrentMovieWidth;	// set to 0 when a new movie is started, don't render until non-0
	int					CurrentMovieHeight;
	int					MovieTextureWidth;
	int					MovieTextureHeight;
    MovieFormat			CurrentMovieFormat;
	int					MovieRotation;
	int					MovieDuration;

	bool				FrameUpdateNeeded;
	int					ClearGhostsFrames;

	GlGeometry			UnitSquare;		// -1 to 1

	// Used to explicitly clear a hole in alpha.
	ovrSurfaceDef		FadedScreenMaskSquareDef;

	ovrSurfaceDef		BlackSceneScreenSurfaceDef;
	ovrSurfaceDef		SceneScreenSurfaceDef;

	ovrSurfaceDef		ScreenSurfaceDef;
	GlBuffer			ScreenTexMatrices;
	Vector4f			ScreenColor[2];		// { UniformColor, ScaleBias }
	GlTexture			ScreenTexture[2];	// { MovieTexture, Fade Texture }

	// We can't directly create a mip map on the OES_external_texture, so
	// it needs to be copied to a conventional texture.
	// It must be triple buffered for use as a TimeWarp overlay plane.
	int					CurrentMipMappedMovieTexture;	// 0 - 2
	ovrTextureSwapChain * MipMappedMovieTextureSwapChain;
	int					MipMappedMovieTextureSwapChainLength;
	GLuint *			MipMappedMovieFBOs;
	Vector2i			BufferSize;						// rebuild if != MovieTextureWidth / Height

	GLuint				ScreenVignetteTexture;
	GLuint				ScreenVignetteSbsTexture;	// for side by side 3D

	sceneProgram_t		SceneProgramIndex;

	OvrSceneView		Scene;
	SceneDef			SceneInfo;
	ovrSurfaceDef *		SceneScreenSurface;		// override this to the movie texture

	static const int	MAX_SEATS = 8;
	Vector3f			SceneSeatPositions[MAX_SEATS];
	int					SceneSeatCount;
	int					SeatPosition;

	Bounds3f			SceneScreenBounds;

	bool 				AllowMove;

	bool				VoidedScene;
	bool				FixVoidedScene;		// For some reason the void screen is placed in the wrong position when
											// we switch to it for videos that are too costly to decode while drawing
											// the scene (which causes judder).  Set this flag to reposition the
											// screen in the Frame function.

private:
	SceneManager &		operator=( const SceneManager & );

	void				CheckForbufferResize();
	GLuint 				BuildScreenVignetteTexture( const int horizontalTile ) const;
	int 				BottomMipLevel( const int width, const int height ) const;
};

} // namespace OculusCinema

#endif // SceneManager_h
