/************************************************************************************

Filename	:	VrCubeWorld_Framework.cpp
Content		:	This sample uses the application framework.
Created		:	March, 2015
Authors		:	J.M.P. van Waveren

Copyright	:	Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "App.h"
#include "GuiSys.h"
#include "OVR_Locale.h"
#include "SoundEffectContext.h"
#include "SystemClock.h"
#include <memory>

#if 0
	#define GL( func )		func; EglCheckErrors();
#else
	#define GL( func )		func;
#endif

/*
================================================================================

VrCubeWorld

================================================================================
*/

namespace OVR
{

static const int CPU_LEVEL			= 2;
static const int GPU_LEVEL			= 3;
static const int NUM_INSTANCES		= 1500;
static const int NUM_ROTATIONS		= 16;
static const Vector4f CLEAR_COLOR( 0.125f, 0.0f, 0.125f, 1.0f );

static const char VERTEX_SHADER[] =
	"in vec3 Position;\n"
	"in vec4 VertexColor;\n"
	"in mat4 VertexTransform;\n"
	"out vec4 fragmentColor;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( VertexTransform * vec4( Position, 1.0 ) ) );\n"
	"	fragmentColor = VertexColor;\n"
	"}\n";

static const char FRAGMENT_SHADER[] =
	"in lowp vec4 fragmentColor;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = fragmentColor;\n"
	"}\n";

// setup Cube
struct ovrCubeVertices
{
	Vector3f positions[8];
	Vector4f colors[8];
};

static ovrCubeVertices cubeVertices =
{
	// positions
	{
		Vector3f( -1.0f, +1.0f, -1.0f ), Vector3f( +1.0f, +1.0f, -1.0f ), Vector3f( +1.0f, +1.0f, +1.0f ), Vector3f( -1.0f, +1.0f, +1.0f ),	// top
		Vector3f( -1.0f, -1.0f, -1.0f ), Vector3f( -1.0f, -1.0f, +1.0f ), Vector3f( +1.0f, -1.0f, +1.0f ), Vector3f( +1.0f, -1.0f, -1.0f )	// bottom
	},
	// colors
	{
		Vector4f( 1.0f, 0.0f, 1.0f, 1.0f ), Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Vector4f( 0.0f, 0.0f, 1.0f, 1.0f ), Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ),
		Vector4f( 0.0f, 0.0f, 1.0f, 1.0f ), Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Vector4f( 1.0f, 0.0f, 1.0f, 1.0f ), Vector4f( 1.0f, 0.0f, 0.0f, 1.0f )
	},
};

static const unsigned short cubeIndices[36] =
{
	0, 2, 1, 2, 0, 3,	// top
	4, 6, 5, 6, 4, 7,	// bottom
	2, 6, 7, 7, 1, 2,	// right
	0, 4, 5, 5, 3, 0,	// left
	3, 5, 6, 6, 2, 3,	// front
	0, 1, 7, 7, 4, 0	// back
};


class VrCubeWorld : public VrAppInterface
{
public:
							VrCubeWorld();
							~VrCubeWorld();

	virtual void 			Configure( ovrSettings & settings );
	virtual void			EnteredVrMode( const ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI );
	virtual void			LeavingVrMode();
	virtual ovrFrameResult	Frame( const ovrFrameInput & vrFrame );

	ovrLocale &				GetLocale() { return *Locale; }

private:
	ovrSoundEffectContext * SoundEffectContext;
	OvrGuiSys::SoundEffectPlayer * SoundEffectPlayer;
	OvrGuiSys *				GuiSys;
	ovrLocale *				Locale;
	ovrSurfaceDef 			SurfaceDef;
	unsigned int			Random;
	GlProgram				Program;
	GlGeometry				Cube;
	GLint					VertexTransformAttribute;
	GLuint					InstanceTransformBuffer;
	ovrVector3f				Rotations[NUM_ROTATIONS];
	ovrVector3f				CubePositions[NUM_INSTANCES];
	int						CubeRotations[NUM_INSTANCES];
	ovrMatrix4f				CenterEyeViewMatrix;
	double					startTime;

	float					RandomFloat();
};

VrCubeWorld::VrCubeWorld() :
	SoundEffectContext( NULL ),
	SoundEffectPlayer( NULL ),
	GuiSys( OvrGuiSys::Create() ),
	Locale( NULL ),
	Random( 2 )
{
	CenterEyeViewMatrix = ovrMatrix4f_CreateIdentity();
}

VrCubeWorld::~VrCubeWorld()
{
	delete SoundEffectPlayer;
	SoundEffectPlayer = NULL;

	delete SoundEffectContext;
	SoundEffectContext = NULL;

	GlProgram::Free( Program );
	Cube.Free();
	GL( glDeleteBuffers( 1, &InstanceTransformBuffer ) );

	OvrGuiSys::Destroy( GuiSys );
}

void VrCubeWorld::Configure( ovrSettings & settings )
{
	settings.CpuLevel = CPU_LEVEL;
	settings.GpuLevel = GPU_LEVEL;
	settings.EyeBufferParms.multisamples = 4;

	settings.RenderMode = RENDERMODE_MULTIVIEW;
}

// Returns a random float in the range [0, 1].
float VrCubeWorld::RandomFloat()
{
	Random = 1664525L * Random + 1013904223L;
	unsigned int rf = 0x3F800000 | ( Random & 0x007FFFFF );
	return (*(float *)&rf) - 1.0f;
}

void VrCubeWorld::EnteredVrMode( const ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI )
{
	OVR_UNUSED( intentFromPackage );
	OVR_UNUSED( intentJSON );
	OVR_UNUSED( intentURI );

	if ( intentType == INTENT_LAUNCH )
	{
		const ovrJava * java = app->GetJava();
		SoundEffectContext = new ovrSoundEffectContext( *java->Env, java->ActivityObject );
		SoundEffectContext->Initialize( &app->GetFileSys() );
		SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

		Locale = ovrLocale::Create( *java->Env, java->ActivityObject, "default" );

		String fontName;
		GetLocale().GetString( "@string/font_name", "efigs.fnt", fontName );
		GuiSys->Init( this->app, *SoundEffectPlayer, fontName.ToCStr(), &app->GetDebugLines() );

		// Create the program.
		Program = GlProgram::Build( VERTEX_SHADER, FRAGMENT_SHADER, NULL, 0 );
		VertexTransformAttribute = glGetAttribLocation( Program.Program, "VertexTransform" );

		// Create the cube.
		VertexAttribs attribs;
		attribs.position.Resize( 8 );
		attribs.color.Resize( 8 );
		for ( int i = 0; i < 8; i++ )
		{
			attribs.position[i] = cubeVertices.positions[i];
			attribs.color[i] = cubeVertices.colors[i];
		}

		Array< TriangleIndex > indices;
		indices.Resize( 36 );
		for ( int i = 0; i < 36; i++ )
		{
			indices[i] = cubeIndices[i];
		}

		Cube.Create( attribs, indices );

		// Setup the instance transform attributes.
		GL( glBindVertexArray( Cube.vertexArrayObject ) );
		GL( glGenBuffers( 1, &InstanceTransformBuffer ) );
		GL( glBindBuffer( GL_ARRAY_BUFFER, InstanceTransformBuffer ) );
		GL( glBufferData( GL_ARRAY_BUFFER, NUM_INSTANCES * 4 * 4 * sizeof( float ), NULL, GL_DYNAMIC_DRAW ) );
		for ( int i = 0; i < 4; i++ )
		{
			GL( glEnableVertexAttribArray( VertexTransformAttribute + i ) );
			GL( glVertexAttribPointer( VertexTransformAttribute + i, 4, GL_FLOAT,
										false, 4 * 4 * sizeof( float ), (void *)( i * 4 * sizeof( float ) ) ) );
			GL( glVertexAttribDivisor( VertexTransformAttribute + i, 1 ) );
		}
		GL( glBindVertexArray( 0 ) );

		// Setup random rotations.
		for ( int i = 0; i < NUM_ROTATIONS; i++ )
		{
			Rotations[i].x = RandomFloat();
			Rotations[i].y = RandomFloat();
			Rotations[i].z = RandomFloat();
		}

		// Setup random cube positions and rotations.
		for ( int i = 0; i < NUM_INSTANCES; i++ )
		{
			volatile float rx, ry, rz;
			for ( ; ; )
			{
				rx = ( RandomFloat() - 0.5f ) * ( 50.0f + static_cast<float>( sqrt( NUM_INSTANCES ) ) );
				ry = ( RandomFloat() - 0.5f ) * ( 50.0f + static_cast<float>( sqrt( NUM_INSTANCES ) ) );
				rz = ( RandomFloat() - 0.5f ) * ( 50.0f + static_cast<float>( sqrt( NUM_INSTANCES ) ) );

				// If too close to 0,0,0
				if ( fabsf( rx ) < 4.0f && fabsf( ry ) < 4.0f && fabsf( rz ) < 4.0f )
				{
					continue;
				}

				// Test for overlap with any of the existing cubes.
				bool overlap = false;
				for ( int j = 0; j < i; j++ )
				{
					if (	fabsf( rx - CubePositions[j].x ) < 4.0f &&
							fabsf( ry - CubePositions[j].y ) < 4.0f &&
							fabsf( rz - CubePositions[j].z ) < 4.0f )
					{
						overlap = true;
						break;
					}
				}

				if ( !overlap )
				{
					break;
				}
			}

			// Insert into list sorted based on distance.
			int insert = 0;
			const float distSqr = rx * rx + ry * ry + rz * rz;
			for ( int j = i; j > 0; j-- )
			{
				const ovrVector3f * otherPos = &CubePositions[j - 1];
				const float otherDistSqr = otherPos->x * otherPos->x + otherPos->y * otherPos->y + otherPos->z * otherPos->z;
				if ( distSqr > otherDistSqr )
				{
					insert = j;
					break;
				}
				CubePositions[j] = CubePositions[j - 1];
				CubeRotations[j] = CubeRotations[j - 1];
			}

			CubePositions[insert].x = rx;
			CubePositions[insert].y = ry;
			CubePositions[insert].z = rz;

			CubeRotations[insert] = (int)( RandomFloat() * ( NUM_ROTATIONS - 0.1f ) );
		}

		// Create SurfaceDef
		SurfaceDef.surfaceName = "VrCubeWorld Framework";
		SurfaceDef.graphicsCommand.Program = Program;
		SurfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
		SurfaceDef.graphicsCommand.GpuState.cullEnable = true;
		SurfaceDef.graphicsCommand.GpuState.depthEnable = true;
		SurfaceDef.geo = Cube;
		SurfaceDef.numInstances = NUM_INSTANCES;

		startTime = SystemClock::GetTimeInSeconds();
	}
	else if ( intentType == INTENT_NEW )
	{
	}
}

void VrCubeWorld::LeavingVrMode()
{
}

ovrFrameResult VrCubeWorld::Frame( const ovrFrameInput & vrFrame )
{
	// process input events first because this mirrors the behavior when OnKeyEvent was
	// a virtual function on VrAppInterface and was called by VrAppFramework.
	for ( int i = 0; i < vrFrame.Input.NumKeyEvents; i++ )
	{
		const int keyCode = vrFrame.Input.KeyEvents[i].KeyCode;
		const int repeatCount = vrFrame.Input.KeyEvents[i].RepeatCount;
		const KeyEventType eventType = vrFrame.Input.KeyEvents[i].EventType;

		if ( GuiSys->OnKeyEvent( keyCode, repeatCount, eventType ) )
		{
			continue;   // consumed the event
		}
		// If nothing consumed the key and it's a short-press of the back key, then exit the application to OculusHome.
		if ( keyCode == OVR_KEY_BACK && eventType == KEY_EVENT_SHORT_PRESS )
		{
			app->ShowSystemUI( VRAPI_SYS_UI_CONFIRM_QUIT_MENU );
			continue;
		}
	}

	Vector3f currentRotation;
	currentRotation.x = (float)( vrFrame.PredictedDisplayTimeInSeconds - startTime );
	currentRotation.y = (float)( vrFrame.PredictedDisplayTimeInSeconds - startTime );
	currentRotation.z = (float)( vrFrame.PredictedDisplayTimeInSeconds - startTime );

	ovrMatrix4f rotationMatrices[NUM_ROTATIONS];
	for ( int i = 0; i < NUM_ROTATIONS; i++ )
	{
		rotationMatrices[i] = ovrMatrix4f_CreateRotation(
								Rotations[i].x * currentRotation.x,
								Rotations[i].y * currentRotation.y,
								Rotations[i].z * currentRotation.z );
	}

	// Update the instance transform attributes.
	GL( glBindBuffer( GL_ARRAY_BUFFER, InstanceTransformBuffer ) );
	GL( Matrix4f * cubeTransforms = (Matrix4f *) glMapBufferRange( GL_ARRAY_BUFFER, 0,
				NUM_INSTANCES * sizeof( Matrix4f ), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT ) );
	for ( int i = 0; i < NUM_INSTANCES; i++ )
	{
		const int index = CubeRotations[i];

		// Write in order in case the mapped buffer lives on write-combined memory.
		cubeTransforms[i].M[0][0] = rotationMatrices[index].M[0][0];
		cubeTransforms[i].M[0][1] = rotationMatrices[index].M[0][1];
		cubeTransforms[i].M[0][2] = rotationMatrices[index].M[0][2];
		cubeTransforms[i].M[0][3] = rotationMatrices[index].M[0][3];

		cubeTransforms[i].M[1][0] = rotationMatrices[index].M[1][0];
		cubeTransforms[i].M[1][1] = rotationMatrices[index].M[1][1];
		cubeTransforms[i].M[1][2] = rotationMatrices[index].M[1][2];
		cubeTransforms[i].M[1][3] = rotationMatrices[index].M[1][3];

		cubeTransforms[i].M[2][0] = rotationMatrices[index].M[2][0];
		cubeTransforms[i].M[2][1] = rotationMatrices[index].M[2][1];
		cubeTransforms[i].M[2][2] = rotationMatrices[index].M[2][2];
		cubeTransforms[i].M[2][3] = rotationMatrices[index].M[2][3];

		cubeTransforms[i].M[3][0] = CubePositions[i].x;
		cubeTransforms[i].M[3][1] = CubePositions[i].y;
		cubeTransforms[i].M[3][2] = CubePositions[i].z;
		cubeTransforms[i].M[3][3] = 1.0f;
	}
	GL( glUnmapBuffer( GL_ARRAY_BUFFER ) );
	GL( glBindBuffer( GL_ARRAY_BUFFER, 0 ) );

	CenterEyeViewMatrix = vrapi_GetViewMatrixFromPose( &vrFrame.Tracking.HeadPose.Pose );

	// Update GUI systems last, but before rendering anything.
	GuiSys->Frame( vrFrame, CenterEyeViewMatrix );

	ovrFrameResult res;
	res.ClearColorBuffer = true;
	res.ClearColor = CLEAR_COLOR;
	res.FrameMatrices.CenterView = CenterEyeViewMatrix;

	res.Surfaces.PushBack( ovrDrawSurface( &SurfaceDef ) );

	// Append GuiSys surfaces.
	GuiSys->AppendSurfaceList( res.FrameMatrices.CenterView, &res.Surfaces );

	res.FrameIndex = vrFrame.FrameNumber;
	res.DisplayTime = vrFrame.PredictedDisplayTimeInSeconds;
	res.SwapInterval = app->GetSwapInterval();

	res.FrameFlags = 0;
	res.LayerCount = 0;

	ovrLayerProjection2 & worldLayer = res.Layers[ res.LayerCount++ ].Projection;
	worldLayer = vrapi_DefaultLayerProjection2();

	worldLayer.HeadPose = vrFrame.Tracking.HeadPose;
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		res.FrameMatrices.EyeView[eye] = vrFrame.Tracking.Eye[eye].ViewMatrix;
		// Calculate projection matrix using custom near plane value.
		res.FrameMatrices.EyeProjection[eye] = ovrMatrix4f_CreateProjectionFov( vrFrame.FovX, vrFrame.FovY, 0.0f, 0.0f, 1.0f, 0.0f );

		worldLayer.Textures[eye].ColorSwapChain = vrFrame.ColorTextureSwapChain[eye];
		worldLayer.Textures[eye].SwapChainIndex = vrFrame.TextureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = vrFrame.TexCoordsFromTanAngles;
	}
	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	return res;
}

} // namespace OVR

#if defined( OVR_OS_ANDROID )
extern "C"
{

long Java_com_oculus_vrcubeworld_MainActivity_nativeSetAppInterface( JNIEnv *jni, jclass clazz, jobject activity,
	jstring fromPackageName, jstring commandString, jstring uriString )
{
	// This is called by the java UI thread.
	LOG( "nativeSetAppInterface" );
	return (new OVR::VrCubeWorld())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

} // extern "C"

#endif
