/************************************************************************************

Filename    :   BeamRenderer.h
Content     :   Class that manages and renders view-oriented beams.
Created     :   October 23, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_BeamRenderer_h )
#define OVR_BeamRenderer_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "SurfaceRender.h"
#include "GlProgram.h"
#include "OVR_Input.h"
#include "TextureAtlas.h"
#include "EaseFunctions.h"

namespace OVR {

//==============================================================
// ovrBeamRenderer
class ovrBeamRenderer
{
public:
	static const int MAX_BEAMS = ( 1ULL << ( sizeof( uint16_t ) * 8 ) ) - 1;
	enum ovrBeamHandle
	{
		INVALID_BEAM_HANDLE = MAX_BEAMS
	};
	typedef TypesafeNumberT< uint16_t, ovrBeamHandle, INVALID_BEAM_HANDLE >	handle_t;

	static float			LIFETIME_INFINITE;

					ovrBeamRenderer();
					~ovrBeamRenderer();

	void		    Init( const int maxBeams, const bool depthTest );
	void		    Shutdown();

	void			Frame( const ovrFrameInput & frame, const Matrix4f & centerViewMatrix,
							const class ovrTextureAtlas & atlas );

	void			SetPose( const Posef & pose );

	void		    RenderEyeView( const Matrix4f & viewMatrix, const Matrix4f & projMatrix, 
							Array< ovrDrawSurface > & surfaceList );

	// If lifeTime == LIFETIME_INFINITE, then the beam will never be automatically removed and
	// it can be referenced by handle. The handle will be returned from this function.
	// If the lifeTime != LIFETIME_INFINITE, then this function will still add the beam (if the
	// max beams has not been reached) but return a handle == MAX_BEAMS
	handle_t	  AddBeam( const ovrFrameInput & frame, const ovrTextureAtlas & atlas,
							const int atlasIndex, const float width,
							const Vector3f & start, const Vector3f & end, 
							const Vector4f & initialColor,
							const float lifeTime );

	// Updates the properties of the beam with the specified handle
	void			UpdateBeam( const ovrFrameInput & frame, 
							const handle_t handle,
							const ovrTextureAtlas & atlas,
							const int atlasIndex, const float width,
							const Vector3f & start, const Vector3f & end, 
							const Vector4f & initialColor );

	// removes the beam with the specified handle
	void			RemoveBeam( handle_t const handle );

private:

	void			UpdateBeamInternal( const ovrFrameInput & frame, 
							const handle_t handle,
							const ovrTextureAtlas & atlas,
							const int atlasIndex, const float width,
							const Vector3f & start, const Vector3f & end, 
							const Vector4f & initialColor,
							const float lifeTime );

	struct ovrBeamInfo
	{
		ovrBeamInfo()
			: StartTime( 0.0 )
			, LifeTime( 0.0f )
			, Width( 0.0f )
			, StartPos( 0.0f )
			, EndPos( 0.0f )
			, InitialColor( 0.0f )
			, TexCoords()
//			, TexCoordVel()
//			, TexCoordAccel()
			, AtlasIndex( 0 )
			, Handle( MAX_BEAMS )
			, EaseFunc( ovrEaseFunc::NONE )
		{
		}

		double				StartTime;
		float				LifeTime;
		float				Width;
		Vector3f			StartPos;
		Vector3f			EndPos;
		Vector4f			InitialColor;
		Vector2f			TexCoords[2];	// tex coords are in the space of the atlas entry
//		Vector2f			TexCoordVel;
//		Vector2f			TexCoordAccel;
		uint16_t			AtlasIndex;			// index in the texture atlas
		handle_t		Handle;
		ovrEaseFunc			EaseFunc;
	};

	ovrSurfaceDef 			Surf;

	Array< ovrBeamInfo > 	BeamInfos;
	Array< handle_t >		ActiveBeams;
	Array< handle_t >		FreeBeams;

	int						MaxBeams;

	GlProgram				LineProgram;
	Matrix4f				ModelMatrix;
};

} // namespace OVR

#endif	// OVR_BeamRenderer_h
