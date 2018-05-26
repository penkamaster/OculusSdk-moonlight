/************************************************************************************

Filename    :   VrController.cpp
Content     :   Trivial use of the application framework.
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrController.h"
#include "GuiSys.h"
#include "OVR_Locale.h"
#include "GazeCursor.h"
#include "ControllerGUI.h"

//#define PERSISTENT_RIBBONS

extern "C" {

jlong Java_com_oculus_vrcontroller_MainActivity_nativeSetAppInterface( JNIEnv * jni, jclass clazz, jobject activity,
		jstring fromPackageName, jstring commandString, jstring uriString )
{
	LOG( "nativeSetAppInterface" );
	return (new OVR::ovrVrController())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

} // extern "C"

namespace OVR {

#if defined( PERSISTENT_RIBBONS )
static const int NUM_RIBBON_POINTS = 1024;
#else
static const int NUM_RIBBON_POINTS = 32;
#endif

static const Vector4f LASER_COLOR( 0.0f, 1.0f, 1.0f, 1.0f );

static const char * PrelitVertexShaderSrc = R"=====(
	attribute highp vec4 Position;
	attribute highp vec2 TexCoord;
	varying lowp vec3 oEye;
	varying highp vec2 oTexCoord;
	vec3 transposeMultiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
			m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
			m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
	}
	void main()
	{
	   	gl_Position = TransformVertex( Position );
   		oTexCoord = TexCoord;
		vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
		oEye = eye - vec3( ModelMatrix * Position );
	}
)=====";

static const char * PrelitFragmentShaderSrc = R"=====(
	uniform sampler2D Texture0;
	varying highp vec2 oTexCoord;
	varying lowp vec3 oEye;
	void main()
	{
		gl_FragColor = texture2D( Texture0, oTexCoord );

		lowp vec3 eyeScaled = oEye * 5.0f;
		lowp float eyeDistSq = eyeScaled.x * eyeScaled.x + eyeScaled.y * eyeScaled.y + eyeScaled.z * eyeScaled.z;
		gl_FragColor.w = eyeDistSq - 0.5f;
	}
)=====";

static const char * LitControllerVertexShaderSrc = R"=====(
	attribute lowp vec4 Position;
	attribute lowp vec4 Normal;
	varying lowp vec4 outVertexColor;
	varying lowp vec3 outNormal;
	varying lowp vec3 oEye;
	vec3 transposeMultiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
			m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
			m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
	}
	void main()
	{
  		outVertexColor = vec4( 1.0, 1.0, 0.5, 1.0 );
  		gl_Position = TransformVertex( Position );
		outNormal = normalize( mat3(sm.ViewMatrix[VIEW_ID]) * ( mat3(ModelMatrix) * Normal.xyz ) );
		vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
		oEye = eye - vec3( ModelMatrix * Position );
	}
)=====";

static const char * LitControllerFragmentShaderSrc = R"=====(
	varying lowp vec4 outVertexColor;
	varying lowp vec3 outNormal;
	varying lowp vec3 oEye;
	void main()
	{
		lowp vec4 lightDir = vec4(1,1,1,1);
		lowp float nDotL = max( dot( normalize( outNormal.xyz ), normalize( lightDir.xyz ) ), 0.0 );
		fragColor.xyz = vec3( nDotL * outVertexColor );

		lowp vec3 eyeScaled = oEye * 5.0f;
		lowp float eyeDistSq = eyeScaled.x * eyeScaled.x + eyeScaled.y * eyeScaled.y + eyeScaled.z * eyeScaled.z;
		gl_FragColor.w = eyeDistSq - 0.5f;
	}
)=====";

static const char * SpecularVertexShaderSrc = R"(
	attribute lowp vec4 Position;
	attribute lowp vec3 Normal;
	attribute lowp vec2 TexCoord;
	varying lowp vec3 oEye;
	varying lowp vec3 oNormal;
	varying lowp vec2 oTexCoord;
	varying lowp float oFade;
	vec3 multiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
			m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
			m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
	}
	vec3 transposeMultiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
			m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
			m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
	}
	void main()
	{
		gl_Position = TransformVertex( Position );
		vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
		oEye = eye - vec3( ModelMatrix * Position );
		oNormal = multiply( ModelMatrix, Normal );
		oTexCoord = TexCoord;

		// fade controller if it is too close to the camera
		lowp float FadeDistanceScale = 5.0f;
		lowp float FadeDistanceOffset = 0.525f;
		oFade = ( sm.ViewMatrix[0] * ( ModelMatrix * Position ) ).z;
		oFade *= FadeDistanceScale;
		oFade *= oFade;
		oFade -= FadeDistanceOffset;
		oFade = clamp( oFade, 0.0f, 1.0f);
	}
)";

static const char * SpecularFragmentWithHighlight = R"(
	uniform sampler2D Texture0;
	uniform lowp vec3 SpecularLightDirection;
	uniform lowp vec3 SpecularLightColor;
	uniform lowp vec3 AmbientLightColor;
	uniform sampler2D ButtonMaskTexture;
	uniform lowp vec4 HighLightMask;
	uniform lowp vec3 HighLightColor;
	varying lowp vec3 oEye;
	varying lowp vec3 oNormal;
	varying lowp vec2 oTexCoord;
	varying lowp float oFade;
	void main()
	{
		lowp vec4 hightLightFilter = texture2D( ButtonMaskTexture, oTexCoord );
		lowp float highLight = hightLightFilter.x * HighLightMask.x;
		highLight += hightLightFilter.y * HighLightMask.y;
		highLight += hightLightFilter.z * HighLightMask.z;
		highLight += hightLightFilter.w * HighLightMask.w;

		lowp vec3 eyeDir = normalize( oEye.xyz );
		lowp vec3 Normal = normalize( oNormal );
		lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
		lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
		lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

		lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
		lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

		lowp float specularPower = 1.0f - diffuse.a;
		specularPower = specularPower * specularPower;

		lowp vec3 H = normalize( SpecularLightDirection + eyeDir ); 
		lowp float nDotH = max( dot( Normal, H ), 0.0 ); 
		lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
		lowp vec3 specularValue = specularIntensity * SpecularLightColor;

		lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
		gl_FragColor.xyz = highLight * HighLightColor + (1.0f - highLight) * controllerColor;
		
		// fade controller if it is too close to the camera
		gl_FragColor.w = oFade;
	}
)";

static const char * SpecularFragmentShaderSrc = R"(
	uniform sampler2D Texture0;
	uniform lowp vec3 SpecularLightDirection;
	uniform lowp vec3 SpecularLightColor;
	uniform lowp vec3 AmbientLightColor;
	uniform sampler2D ButtonMaskTexture;
	varying lowp vec3 oEye;
	varying lowp vec3 oNormal;
	varying lowp vec2 oTexCoord;
	varying lowp float oFade;
	void main()
	{

		lowp vec3 eyeDir = normalize( oEye.xyz );
		lowp vec3 Normal = normalize( oNormal );
		lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
		lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
		lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

		lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
		lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

		lowp float specularPower = 1.0f - diffuse.a;
		specularPower = specularPower * specularPower;

		lowp vec3 H = normalize( SpecularLightDirection + eyeDir ); 
		lowp float nDotH = max( dot( Normal, H ), 0.0 ); 
		lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
		lowp vec3 specularValue = specularIntensity * SpecularLightColor;

		lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
		gl_FragColor.xyz = controllerColor;
		
		// fade controller if it is too close to the camera
		gl_FragColor.w = oFade;
	}
)";

static void SetObjectColor( OvrGuiSys & guiSys, VRMenu * menu, char const * name, Vector4f const & color )
{
	VRMenuObject * obj = menu->ObjectForName( guiSys, name );
	if ( obj != nullptr )
	{
		obj->SetSurfaceColor( 0, color );
	}
}

static void SetObjectText( OvrGuiSys & guiSys, VRMenu * menu, char const * name, char const * fmt, ... )
{
	VRMenuObject * obj = menu->ObjectForName( guiSys, name );
	if ( obj != nullptr )
	{
		char text[1024];
		va_list argPtr;
		va_start( argPtr, fmt );
		OVR_vsprintf( text, sizeof( text ), fmt, argPtr );
		va_end( argPtr );
		obj->SetText( text );
	}
}

static void SetObjectVisible( OvrGuiSys & guiSys, VRMenu * menu, char const * name, const bool visible )
{
	VRMenuObject * obj = menu->ObjectForName( guiSys, name );
	if ( obj != nullptr )
	{
		obj->SetVisible( visible );
	}
}

static inline Vector3f ovrMatrix4f_GetTranslation( const ovrMatrix4f & matrix )
{
	ovrVector3f t;
	t.x = matrix.M[0][3];
	t.y = matrix.M[1][3];
	t.z = matrix.M[2][3];
	return t;
}

#if !defined( PERSISTENT_RIBBONS )
static void UpdateRibbon( ovrPointList & points, ovrPointList & velocities, const Vector3f & anchorPos, const float maxDist, const float deltaSeconds )
{
	const Vector3f g( 0.0f, -9.8f, 0.0f );
	const float invDeltaSeconds = 1.0f / deltaSeconds;

	int count = 0;
	int i = points.GetFirst(); 
	Vector3f & firstPoint = points.Get( i );

	Vector3f delta = anchorPos - firstPoint;

	firstPoint = anchorPos;	// move the first point
	//translate( firstPoint, Vector3f( 0.0f, -1.0f, 0.0f ), deltaSeconds );

	Vector3f prevPoint = firstPoint;

	// move and accelerate all subsequent points
	for ( ; ; )
	{
		i = points.GetNext( i );
		if ( i < 0 )
		{
			break;
		}

		count++;

		Vector3f & curPoint = points.Get( i );
		Vector3f & curVel = velocities.Get( i );
		curVel += g * deltaSeconds;
		curPoint = curPoint + curVel * deltaSeconds;

		delta = curPoint - prevPoint;
		const float dist = delta.Length();
		Vector3f dir = delta * 1.0f / dist;
		if ( dist > maxDist )
		{
			Vector3f newPoint = prevPoint + dir * maxDist;
			Vector3f dragDelta = newPoint - curPoint;
			Vector3f dragVel = dragDelta * invDeltaSeconds;
			curVel = dragVel * 0.1f;
			curPoint = newPoint;
		}
		else
		{
			// damping
			curVel = curVel * 0.995f;
		}

		prevPoint = curPoint;
	}

//	LOG( "Ribbon: Updated %i points", count );
}
#endif

//==============================================================================================
// ovrControllerRibbon

//==============================
// ovrControllerRibbon::ovrControllerRibbon
ovrControllerRibbon::ovrControllerRibbon( const int numPoints, const float width, const float length, const Vector4f & color )
	: NumPoints( numPoints )
	, Length( length )
{
#if defined( PERSISTENT_RIBBONS )
	Points = new ovrPointList_Circular( numPoints );
#else
	Points = new ovrPointList_Vector( numPoints );
	Velocities = new ovrPointList_Vector( numPoints );

	for ( int i = 0; i < numPoints; ++i )
	{
		Points->AddToTail( Vector3f( 0.0f, i * ( length / numPoints ), 0.0f ) );
		Velocities->AddToTail( Vector3f( 0.0f ) );
	}
#endif

	Ribbon = new ovrRibbon( *Points, width, color );	
}

ovrControllerRibbon::~ovrControllerRibbon()
{
	delete Points;
	Points = nullptr;
	delete Velocities;
	Velocities = nullptr;
	delete Ribbon;
	Ribbon = nullptr;
}

//==============================
// ovrControllerRibbon::Update
void ovrControllerRibbon::Update( const Matrix4f & centerViewMatrix, const Vector3f & anchorPos, const float deltaSeconds )
{
	OVR_ASSERT( Points != nullptr );
#if defined( PERSISTENT_RIBBONS )
	if ( Points->GetCurPoints() == 0 )
	{
		Points->AddToTail( anchorPos );
	}
	else
	{
		Vector3f delta = anchorPos - Points->Get( Points->GetLast());
		if ( delta.Length() > 0.01f )
		{
			if ( Points->IsFull() )
			{
				Points->RemoveHead();
			}
			Points->AddToTail( anchorPos );
		}
	}
#else
	OVR_ASSERT( Velocities != nullptr );
	UpdateRibbon( *Points, *Velocities, anchorPos, Length / Points->GetMaxPoints(), deltaSeconds );
#endif
	Ribbon->Update( *Points, centerViewMatrix, true );
}

//==============================
// ovrVrController::ovrVrController
ovrVrController::ovrVrController()
	: SoundEffectContext( nullptr )
	, SoundEffectPlayer( nullptr )
	, GuiSys( OvrGuiSys::Create() )
	, Locale( nullptr )
	, SceneModel( nullptr )
	, SpriteAtlas( nullptr )
	, ParticleSystem( nullptr )
	, BeamAtlas( nullptr )
	, RemoteBeamRenderer( nullptr )
	, LaserPointerBeamHandle()
	, LaserPointerParticleHandle()
	, LaserHit( false )
	, ControllerModelGear( nullptr )
	, ControllerModelGearPreLit( nullptr )
	, ControllerModelOculusGo( nullptr )
	, ControllerModelOculusGoPreLit( nullptr )
	, LastGamepadUpdateTimeInSeconds( 0 )
	, Ribbons{ nullptr, nullptr }
{
}

//==============================
// ovrVrController::~ovrVrController
ovrVrController::~ovrVrController()
{
	for ( int i = 0; i < ovrArmModel::HAND_MAX; ++i )
	{
		delete Ribbons[i];
		Ribbons[i] = nullptr;
	}

	delete ControllerModelGear;
	ControllerModelGear = nullptr;
	delete ControllerModelGearPreLit;
	ControllerModelGearPreLit = nullptr;
	delete ControllerModelOculusGo;
	ControllerModelOculusGo = nullptr;
	delete ControllerModelOculusGoPreLit;
	ControllerModelOculusGoPreLit = nullptr;

	delete RemoteBeamRenderer;
	RemoteBeamRenderer = nullptr;

	delete ParticleSystem;
	ParticleSystem = nullptr;

	delete SpriteAtlas;
	SpriteAtlas = nullptr;

	delete SoundEffectPlayer;
	SoundEffectPlayer = nullptr;

	delete SoundEffectContext;
	SoundEffectContext = nullptr;

	OvrGuiSys::Destroy( GuiSys );
	if ( SceneModel != nullptr )
	{
		delete SceneModel;
	}
}

//==============================
// ovrVrController::Configure
void ovrVrController::Configure( ovrSettings & settings )
{
	settings.CpuLevel = 2;
	settings.GpuLevel = 2;

	settings.RenderMode = RENDERMODE_MULTIVIEW;
}

//==============================
// ovrVrController::EnteredVrMode
void ovrVrController::EnteredVrMode( const ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI )
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

		static ovrProgramParm LitUniformParms[] =
		{
			{ "Texture0",				ovrProgramParmType::TEXTURE_SAMPLED },
			{ "SpecularLightDirection",	ovrProgramParmType::FLOAT_VECTOR3 },
			{ "SpecularLightColor",		ovrProgramParmType::FLOAT_VECTOR3 },
			{ "AmbientLightColor",		ovrProgramParmType::FLOAT_VECTOR3 },
		};

		static ovrProgramParm LitWithHighlightUniformParms[] =
		{
			{ "Texture0",				ovrProgramParmType::TEXTURE_SAMPLED },
			{ "SpecularLightDirection",	ovrProgramParmType::FLOAT_VECTOR3 },
			{ "SpecularLightColor",		ovrProgramParmType::FLOAT_VECTOR3 },
			{ "AmbientLightColor",		ovrProgramParmType::FLOAT_VECTOR3 },
			{ "ButtonMaskTexture",		ovrProgramParmType::TEXTURE_SAMPLED },
			{ "HighLightMask",			ovrProgramParmType::FLOAT_VECTOR4 },
			{ "HighLightColor",			ovrProgramParmType::FLOAT_VECTOR3 },
		};

		SpecularLightDirection = Vector3f( 0.0f, 1.0f, 0.0f );
		SpecularLightColor = Vector3f( 1.0f, 1.0f, 1.0f ) * 0.75f;
		AmbientLightColor = Vector3f( 1.0f, 1.0f, 1.0f ) * 0.85f;
		HighLightMask = Vector4f( 0.0f, 0.0f, 0.0f, 0.0f );
		HighLightColor = Vector3f( 1.0f, 0.55f, 0.0f ) *1.5f;

		ProgSingleTexture = BuildProgram( PrelitVertexShaderSrc, PrelitFragmentShaderSrc );

		ProgLitController = BuildProgram( LitControllerVertexShaderSrc, LitControllerFragmentShaderSrc );

		ProgLitSpecularWithHighlight = GlProgram::Build( SpecularVertexShaderSrc, SpecularFragmentWithHighlight,
			LitWithHighlightUniformParms, sizeof( LitWithHighlightUniformParms ) / sizeof( ovrProgramParm ) );

		ProgLitSpecular = GlProgram::Build( SpecularVertexShaderSrc, SpecularFragmentShaderSrc,
			LitUniformParms, sizeof( LitUniformParms ) / sizeof( ovrProgramParm ) );

		{
			ModelGlPrograms	programs;
			const char * controllerModelFile = "apk:///assets/gearcontroller.ovrscene";
			programs.ProgSingleTexture = &ProgLitSpecularWithHighlight;
			programs.ProgBaseColorPBR = &ProgLitSpecularWithHighlight;
			programs.ProgLightMapped = &ProgLitSpecularWithHighlight;
			MaterialParms	materials;
			ControllerModelGear = LoadModelFile( app->GetFileSys(), controllerModelFile, programs, materials );
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[0].Data = &ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.uniformTextures[0];
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[1].Data = &SpecularLightDirection;
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[2].Data = &SpecularLightColor;
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[3].Data = &AmbientLightColor;
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[4].Data = &ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.uniformTextures[1];
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[5].Data = &HighLightMask;
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[6].Data = &HighLightColor;

			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
			ControllerModelGear->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

			if ( ControllerModelGear == NULL || ControllerModelGear->Models.GetSizeI() < 1 )
			{
				FAIL( "Couldn't load Gear VR controller model" );
			}
		}

		{
			ModelGlPrograms	programs;
			const char * controllerModelFile = "apk:///assets/gearcontroller_prelit.ovrscene";
			programs.ProgSingleTexture = &ProgSingleTexture;
			programs.ProgBaseColorPBR = &ProgSingleTexture;
			MaterialParms	materials;
			ControllerModelGearPreLit = LoadModelFile( app->GetFileSys(), controllerModelFile, programs, materials );

			ControllerModelGearPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.depthEnable = ControllerModelGearPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.depthMaskEnable = false;
			ControllerModelGearPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE_SEPARATE;
			ControllerModelGearPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
			ControllerModelGearPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

			if ( ControllerModelGearPreLit == NULL || ControllerModelGearPreLit->Models.GetSizeI() < 1 )
			{
				FAIL( "Couldn't load prelit Gear VR controller model" );
			}
		}

		{
			ModelGlPrograms	programs;
			const char * controllerModelFile = "apk:///assets/oculusgo_controller.ovrscene";
			programs.ProgSingleTexture = &ProgLitSpecularWithHighlight;
			programs.ProgBaseColorPBR = &ProgLitSpecularWithHighlight;
			programs.ProgLightMapped = &ProgLitSpecularWithHighlight;
			MaterialParms	materials;
			ControllerModelOculusGo = LoadModelFile( app->GetFileSys(), controllerModelFile, programs, materials );
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[0].Data = &ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.uniformTextures[0];
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[1].Data = &SpecularLightDirection;
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[2].Data = &SpecularLightColor;
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[3].Data = &AmbientLightColor;
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[4].Data = &ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.uniformTextures[1];
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[5].Data = &HighLightMask;
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.UniformData[6].Data = &HighLightColor;

			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
			ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

			if ( ControllerModelOculusGo == NULL || ControllerModelOculusGo->Models.GetSizeI() < 1 )
			{
				FAIL( "Couldn't load oculus go controller model" );
			}
		}

		{
			ModelGlPrograms	programs;
			const char * controllerModelFile = "apk:///assets/oculusgo_controller_prelit.ovrscene";
			programs.ProgSingleTexture = &ProgSingleTexture;
			programs.ProgBaseColorPBR = &ProgSingleTexture;
			MaterialParms	materials;
			ControllerModelOculusGoPreLit = LoadModelFile( app->GetFileSys(), controllerModelFile, programs, materials );
			
			ControllerModelOculusGoPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
			ControllerModelOculusGoPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
			ControllerModelOculusGoPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

			if ( ControllerModelOculusGoPreLit == NULL || ControllerModelOculusGoPreLit->Models.GetSizeI() < 1 )
			{
				FAIL( "Couldn't load prelit oculus go controller model" );
			}
		}

		{
			MaterialParms materialParms;
			materialParms.UseSrgbTextureFormats = false;
			const char * sceneUri = "apk:///assets/box.ovrscene";
			SceneModel = LoadModelFile( app->GetFileSys(), sceneUri, Scene.GetDefaultGLPrograms(), materialParms );
			//SceneModel = nullptr;

			if ( SceneModel != nullptr )
			{
				Scene.SetWorldModel( *SceneModel );
				Vector3f modelOffset;
				modelOffset.x = 0.5f;
				modelOffset.y = 0.0f;
				modelOffset.z = -1.0f;
				Scene.GetWorldModel()->State.SetMatrix( Matrix4f::Scaling( 2.5f, 2.5f, 2.5f ) * Matrix4f::Translation( modelOffset ) );
			}
		}

		//------------------------------------------------------------------------------------------

		SpriteAtlas = new ovrTextureAtlas();
		SpriteAtlas->Init( app->GetFileSys(), "apk:///assets/particles2.ktx" );
		SpriteAtlas->BuildSpritesFromGrid( 4, 2, 8 );

		ParticleSystem = new ovrParticleSystem();
		ParticleSystem->Init( 2048, *SpriteAtlas, ovrParticleSystem::GetDefaultGpuState(), false );

		BeamAtlas = new ovrTextureAtlas();
		BeamAtlas->Init( app->GetFileSys(), "apk:///assets/beams.ktx" );
		BeamAtlas->BuildSpritesFromGrid( 2, 1, 2 );

		RemoteBeamRenderer = new ovrBeamRenderer();
		RemoteBeamRenderer->Init( 256, true );

		//------------------------------------------------------------------------------------------

		for ( int i = 0; i < ovrArmModel::HAND_MAX; ++i )
		{
			Ribbons[i] = new ovrControllerRibbon( NUM_RIBBON_POINTS, 0.025f, 1.0f, Vector4f( 0.0f, 0.5f, 1.0f, 1.0f ) );
		}

		//------------------------------------------------------------------------------------------

		Menu = ovrControllerGUI::Create( *this );
		if ( Menu != nullptr )
		{
			GuiSys->AddMenu( Menu );
			GuiSys->OpenMenu( Menu->GetName() );

			const ovrDeviceType deviceType = ( ovrDeviceType )vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_DEVICE_TYPE );
			if ( deviceType >= VRAPI_DEVICE_TYPE_OCULUSGO_START && deviceType <= VRAPI_DEVICE_TYPE_OCULUSGO_END )
			{
				SetObjectText( *GuiSys, Menu, "panel", "VrController (Oculus Go)" );
			}
		}
	}
	else if ( intentType == INTENT_NEW )
	{
	}

	LastGamepadUpdateTimeInSeconds = 0.0;
}

//==============================
// ovrVrController::ResetLaserPointer
void ovrVrController::ResetLaserPointer()
{
	if ( LaserPointerBeamHandle.IsValid() )
	{
		RemoteBeamRenderer->RemoveBeam( LaserPointerBeamHandle );
		LaserPointerBeamHandle.Release();
	}
	if ( LaserPointerParticleHandle.IsValid() )
	{
		ParticleSystem->RemoveParticle( LaserPointerParticleHandle );
		LaserPointerParticleHandle.Release();
	}

	// Show the gaze cursor when the remote laser pointer is not active.
	GuiSys->GetGazeCursor().ShowCursor();
}

//==============================
// ovrVrController::LeavingVrMode
void ovrVrController::LeavingVrMode()
{
	LOG( "LeavingVrMode" );
	for ( int i = InputDevices.size() - 1; i >= 0; --i )
	{
		OnDeviceDisconnected( InputDevices[i]->GetDeviceID() );
	}

	ResetLaserPointer();
}

//==============================
// ovrVrController::OnKeyEvent
bool ovrVrController::OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType )
{
	if ( GuiSys->OnKeyEvent( keyCode, repeatCount, eventType ) )
	{
		return true;
	}
	return false;
}

static void RenderBones( const ovrFrameInput & frame, ovrParticleSystem * ps, ovrTextureAtlas & particleAtlas,
		const uint16_t particleAtlasIndex, ovrBeamRenderer * br, ovrTextureAtlas & beamAtlas, const uint16_t beamAtlasIndex, 
		const Posef & worldPose, const Array< ovrJoint > & joints, 
		Array< ovrPairT< ovrParticleSystem::handle_t, ovrBeamRenderer::handle_t > > & handles )
{
	for ( int i = 0; i < joints.GetSizeI(); ++i )
	{
		const ovrJoint & joint = joints[i];
		const Posef jw = worldPose * joint.Pose;

		if ( !handles[i].First.IsValid() )
		{
			handles[i].First = ps->AddParticle( frame, jw.Translation, 0.0f, Vector3f( 0.0f ), Vector3f( 0.0f ), 
					joint.Color, ovrEaseFunc::NONE, 0.0f, 0.08f, FLT_MAX, particleAtlasIndex );
		}
		else
		{
			ps->UpdateParticle( frame, handles[i].First, jw.Translation, 0.0f, Vector3f( 0.0f ), Vector3f( 0.0f ), joint.Color, 
				ovrEaseFunc::NONE, 0.0f, 0.08f, FLT_MAX, particleAtlasIndex );
		}

		if ( i > 0 )
		{
			const ovrJoint & parentJoint = joints[joint.ParentIndex];
			const Posef pw = worldPose * parentJoint.Pose;
			if ( !handles[i].Second.IsValid() )
			{
				handles[i].Second = br->AddBeam( frame, beamAtlas, beamAtlasIndex, 0.064f, pw.Translation, jw.Translation, 
						Vector4f( 0.0f, 0.0f, 1.0f, 1.0f ), ovrBeamRenderer::LIFETIME_INFINITE );
			}
			else
			{
				br->UpdateBeam( frame, handles[i].Second, beamAtlas, beamAtlasIndex, 0.064f, pw.Translation, jw.Translation, 
						Vector4f( 0.0f, 0.0f, 1.0f, 1.0f ) );
			}
		}
	}
}

static void ResetBones( ovrParticleSystem * ps, ovrBeamRenderer * br, jointHandles_t & handles )
{
    for ( int i = 0; i < handles.GetSizeI(); ++i )
    {
        if ( handles[i].First.IsValid() )
        {
            ps->RemoveParticle( handles[i].First );
            handles[i].First.Release();
        }
        if ( handles[i].Second.IsValid() )
        {
            br->RemoveBeam( handles[i].Second );
            handles[i].Second.Release();
        }
    }
}

static void TrackpadStats( const Vector2f & pos, const Vector2f & range, 
	const Vector2f & size, Vector2f & min, Vector2f & max, Vector2f & mm )
{
	if ( pos.x < min.x ) { min.x = pos.x; }
	if ( pos.y < min.y ) { min.y = pos.y; }
	if ( pos.x > max.x ) { max.x = pos.x; }
	if ( pos.y > max.y ) { max.y = pos.y; }

	const Vector2f trackpadNormalized( pos.x / range.x, pos.y / range.y );
	mm = Vector2f( trackpadNormalized.x * size.x, trackpadNormalized.x * size.y );
}

//==============================
// ovrVrController::Frame
ovrFrameResult ovrVrController::Frame( const ovrFrameInput & vrFrame )
{
	// process input events first because this mirrors the behavior when OnKeyEvent was
	// a virtual function on VrAppInterface and was called by VrAppFramework.
	for ( int i = 0; i < vrFrame.Input.NumKeyEvents; i++ )
	{
		const int keyCode = vrFrame.Input.KeyEvents[i].KeyCode;
		const int repeatCount = vrFrame.Input.KeyEvents[i].RepeatCount;
		const KeyEventType eventType = vrFrame.Input.KeyEvents[i].EventType;

		if ( OnKeyEvent( keyCode, repeatCount, eventType ) )
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

	// disallow player movement
	ovrFrameInput vrFrameWithoutMove = vrFrame;

	vrFrameWithoutMove.Input.sticks[0][0] = 0.0f;
	vrFrameWithoutMove.Input.sticks[0][1] = 0.0f;

	//------------------------------------------------------------------------------------------
	EnumerateInputDevices();
	bool recenteredController = false;
	ovrArmModel::ovrHandedness dominantHand =
		vrapi_GetSystemPropertyInt( app->GetJava(), VRAPI_SYS_PROP_DOMINANT_HAND ) == VRAPI_HAND_LEFT ? ovrArmModel::HAND_LEFT : ovrArmModel::HAND_RIGHT;
    bool hasActiveController = false;

	bool showHeadset = true;


	ClearAndHideMenuItems();

	// for each device, query its current tracking state and input state
	// it's possible for a device to be removed during this loop, so we go through it backwards
	for ( int i = (int)InputDevices.size() - 1; i >= 0; --i )
	{
		ovrInputDeviceBase * device = InputDevices[i];
		if ( device == nullptr )
		{
			OVR_ASSERT( false );	// this should never happen!
			continue;
		}
		ovrDeviceID deviceID = device->GetDeviceID();
		if ( deviceID == ovrDeviceIdType_Invalid )
		{
			OVR_ASSERT( deviceID != ovrDeviceIdType_Invalid );
			continue;
		}
		if ( device->GetType() == ovrControllerType_Headset && showHeadset )
		{
			ovrInputDevice_Headset & hsDevice = *static_cast< ovrInputDevice_Headset*>( device );
			ovrTracking hmtTracking;
			ovrResult result = vrapi_GetInputTrackingState( app->GetOvrMobile(), deviceID, vrFrame.PredictedDisplayTimeInSeconds, &hmtTracking );

			if ( result != ovrSuccess )
			{
				LOG_WITH_TAG( "HMTPose", "Error %i getting HMT tracking!", result );
				OnDeviceDisconnected( deviceID );
				SetObjectText( *GuiSys, Menu, "secondary_input_header", "Headset Error" );
				continue;
			}		
			else
			{
				SetObjectText( *GuiSys, Menu, "secondary_input_header", "Gear VR Headset" );

				const ovrInputHeadsetCapabilities* headsetCapabilities = reinterpret_cast<const ovrInputHeadsetCapabilities*>( hsDevice.GetCaps() );

				String buttonStr = "";
				if ( headsetCapabilities->ButtonCapabilities & ovrButton_A ) 
				{ 
					buttonStr += "TRIGGER "; 
					SetObjectVisible( *GuiSys, Menu, "secondary_input_trigger", true );
					SetObjectVisible( *GuiSys, Menu, "secondary_input_triggerana", true );
				}

				if ( headsetCapabilities->ButtonCapabilities & ovrButton_Enter )
				{
					buttonStr += "TP(Click) ";
					SetObjectVisible( *GuiSys, Menu, "secondary_input_touchpad_click", true );
				}

				if ( headsetCapabilities->ButtonCapabilities & ovrButton_Back ) 
				{
					buttonStr += "BACK ";
					SetObjectVisible( *GuiSys, Menu, "secondary_input_back", true );
				}

				SetObjectVisible( *GuiSys, Menu, "secondary_input_range_caps", true );
				SetObjectText( *GuiSys, Menu, "secondary_input_range_caps", "Touch Range: ( %i, %i )",
					( int )headsetCapabilities->TrackpadMaxX, ( int )headsetCapabilities->TrackpadMaxY );

				SetObjectVisible( *GuiSys, Menu, "secondary_input_size_caps", true );
				SetObjectText( *GuiSys, Menu, "secondary_input_size_caps", "Touch Size: ( %.2f, %.2f )",
					headsetCapabilities->TrackpadSizeX, headsetCapabilities->TrackpadSizeY );

				SetObjectVisible( *GuiSys, Menu, "secondary_input_button_caps", true );
				SetObjectText( *GuiSys, Menu, "secondary_input_button_caps", "Buttons: %s",
					buttonStr.ToCStr() );

				float yaw;
				float pitch;
				float roll;
				Quatf r( hmtTracking.HeadPose.Pose.Orientation );
				r.GetEulerAngles< Axis_Y, Axis_X, Axis_Z >( &yaw, &pitch, &roll );
				//LOG_WITH_TAG( "HMTPose", "Pose.r = ( %.2f, %.2f, %.2f, %.2f ), ypr( %.2f, %.2f, %.2f )", 
				//		r.x, r.y, r.z, r.w, 
				//		MATH_FLOAT_RADTODEGREEFACTOR * yaw, MATH_FLOAT_RADTODEGREEFACTOR* pitch, MATH_FLOAT_RADTODEGREEFACTOR * roll );

				ovrInputStateHeadset headsetInputState;
				headsetInputState.Header.ControllerType = ovrControllerType_Headset;
				result = vrapi_GetCurrentInputState( app->GetOvrMobile(), deviceID, &headsetInputState.Header );
				if ( result != ovrSuccess )
				{
					LOG_WITH_TAG( "HeadsetState", "ERROR %i getting HMT input state!", result );
					OnDeviceDisconnected( deviceID );
				}
				else
				{
					String buttons;
					if ( headsetInputState.Buttons & ovrButton_Back )
					{
						buttons += " BACK";
						SetObjectColor( *GuiSys, Menu, "secondary_input_back", Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					SetObjectVisible( *GuiSys, Menu, "secondary_input_touch", true );
					if ( headsetInputState.TrackpadStatus )
					{
						SetObjectColor( *GuiSys, Menu, "secondary_input_touch", Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					static Vector2f minTrackpad( FLT_MAX );
					static Vector2f maxTrackpad( -FLT_MAX );
					Vector2f mm( 0.0f );
					TrackpadStats( headsetInputState.TrackpadPosition, Vector2f( hsDevice.GetHeadsetCaps().TrackpadMaxX, hsDevice.GetHeadsetCaps().TrackpadMaxY ),
							Vector2f( hsDevice.GetHeadsetCaps().TrackpadSizeX, hsDevice.GetHeadsetCaps().TrackpadSizeY ), minTrackpad, maxTrackpad, mm );

					SetObjectVisible( *GuiSys, Menu, "secondary_input_touch_pos", true );
					SetObjectText( *GuiSys, Menu, "secondary_input_touch_pos", "Pos( %.2f, %.2f ) Min( %.2f, %.2f ) Max( %.2f, %.2f )", 
						headsetInputState.TrackpadPosition.x, headsetInputState.TrackpadPosition.y,
						minTrackpad.x, minTrackpad.y, maxTrackpad.x, maxTrackpad.y );
		/*
					LOG_WITH_TAG( "Buttons", "%s, trackpad = %s", 
							buttons.ToCStr(), headsetInputState.TrackpadStatus ? "down" : "up" );
					LOG_WITH_TAG( "Trackpad", "headset ( %.2f, %.2f ), mm( %.2f, %.2f ), minmax( %.2f, %.2f, %.2f, %.2f )", 					
							headsetInputState.TrackpadPosition.x, headsetInputState.TrackpadPosition.y,
							mm.x, mm.y, minTrackpad.x, minTrackpad.y, maxTrackpad.x, maxTrackpad.y );
		*/
				}
			}
		}
		else if ( device->GetType() == ovrControllerType_TrackedRemote )
		{
			ovrInputDevice_TrackedRemote & trDevice = *static_cast< ovrInputDevice_TrackedRemote*>( device );
					
			if ( deviceID != ovrDeviceIdType_Invalid )
			{
				ovrTracking remoteTracking;
				ovrResult result = vrapi_GetInputTrackingState( app->GetOvrMobile(), deviceID, vrFrame.PredictedDisplayTimeInSeconds, &remoteTracking );
				if ( result != ovrSuccess )
				{
					OnDeviceDisconnected( deviceID );
					continue;
				}

				trDevice.SetTracking( remoteTracking );

				float yaw;
				float pitch;
				float roll;
				Quatf r( remoteTracking.HeadPose.Pose.Orientation );
				r.GetEulerAngles< Axis_Y, Axis_X, Axis_Z >( &yaw, &pitch, &roll );
				//LOG_WITH_TAG( "MLBUPose", "Pose.r = ( %.2f, %.2f, %.2f, %.2f ), ypr( %.2f, %.2f, %.2f ), t( %.2f, %.2f, %.2f )", 
				//	r.x, r.y, r.z, r.w, 
				//	MATH_FLOAT_RADTODEGREEFACTOR * yaw, MATH_FLOAT_RADTODEGREEFACTOR * pitch, MATH_FLOAT_RADTODEGREEFACTOR * roll,
				//	remoteTracking.HeadPose.Pose.Position.x, remoteTracking.HeadPose.Pose.Position.y, remoteTracking.HeadPose.Pose.Position.z );

				vrapi_SetRemoteEmulation( app->GetOvrMobile(), false );
				
				result = PopulateRemoteControllerInfo( trDevice, recenteredController );
				if ( result == ovrSuccess )
				{
					hasActiveController = true;
				}
			}
		}
		else if ( device->GetType() == ovrControllerType_Gamepad )
		{
			if ( deviceID != ovrDeviceIdType_Invalid )
			{
				ovrInputStateGamepad gamepadInputState;
				gamepadInputState.Header.ControllerType = ovrControllerType_Gamepad;
				ovrResult result = vrapi_GetCurrentInputState( app->GetOvrMobile(), deviceID, &gamepadInputState.Header );
				if ( result == ovrSuccess && gamepadInputState.Header.TimeInSeconds >= LastGamepadUpdateTimeInSeconds )
				{
					LastGamepadUpdateTimeInSeconds = gamepadInputState.Header.TimeInSeconds;

					SetObjectVisible( *GuiSys, Menu, "tertiary_input_l1", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_l2", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_r1", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_r2", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_x", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_y", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_a", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_b", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_dpad", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_lstick", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_rstick", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_back", true );
					SetObjectVisible( *GuiSys, Menu, "tertiary_input_header", true );

					if ( gamepadInputState.Buttons & ovrButton_Enter )
					{
						SetObjectText( *GuiSys, Menu, "tertiary_input_header", "Gamepad ID: %u  START PRESSED", deviceID );
					}
					else
					{
						SetObjectText( *GuiSys, Menu, "tertiary_input_header", "Gamepad ID: %u", deviceID );
					}

					SetObjectText( *GuiSys, Menu, "tertiary_input_lstick", "LStick x: %.2f y: %.2f", gamepadInputState.LeftJoyStick.x, gamepadInputState.LeftJoyStick.y );
					SetObjectText( *GuiSys, Menu, "tertiary_input_rstick", "RStick x: %.2f y: %.2f", gamepadInputState.RightJoyStick.x, gamepadInputState.RightJoyStick.y );
					SetObjectText( *GuiSys, Menu, "tertiary_input_l2", "L2 %.2f", gamepadInputState.LeftTrigger );
					SetObjectText( *GuiSys, Menu, "tertiary_input_r2", "R2 %.2f", gamepadInputState.RightTrigger );

					String dpadStr = "DPAD ";
					bool dpadset = false;
					if ( gamepadInputState.Buttons & ovrButton_Up )
					{
						dpadset = true;
						dpadStr += " UP";
					}
					if ( gamepadInputState.Buttons & ovrButton_Down )
					{
						dpadset = true;
						dpadStr += " DOWN";
					}
					if ( gamepadInputState.Buttons & ovrButton_Left )
					{
						dpadset = true;
						dpadStr += " LEFT";
					}
					if ( gamepadInputState.Buttons & ovrButton_Right )
					{
						dpadset = true;
						dpadStr += " RIGHT";
					}
					SetObjectText( *GuiSys, Menu, "tertiary_input_dpad", "%s", dpadStr.ToCStr() );
					if( dpadset )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_dpad",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_LShoulder )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_l1",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_RShoulder )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_r1",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_X )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_x",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_Y )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_y",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_A )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_a",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_B )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_b",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_LThumb )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_lstick",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_RThumb )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_rstick",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.Buttons & ovrButton_Back )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_back",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if( gamepadInputState.LeftTrigger >= 1.0f )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_l2",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}

					if ( gamepadInputState.RightTrigger >= 1.0f )
					{
						SetObjectColor( *GuiSys, Menu, "tertiary_input_r2",
							Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
					}
				}
			}
		}
		else
		{
			LOG_WITH_TAG( "MLBUState", "Unexpected Device Type %d on %d", device->GetType(), i );
		}
	}

	//------------------------------------------------------------------------------------------

	// if the orientation is tracked by the headset, don't allow the gamepad to rotate the view
	if ( ( vrFrameWithoutMove.Tracking.Status & VRAPI_TRACKING_STATUS_ORIENTATION_TRACKED ) != 0 )
	{
		vrFrameWithoutMove.Input.sticks[1][0] = 0.0f;
		vrFrameWithoutMove.Input.sticks[1][1] = 0.0f;	
	}

	// Player movement.
	Scene.SetFreeMove( true );
	Scene.Frame( vrFrameWithoutMove );

	ovrFrameResult res;
	Scene.GetFrameMatrices( vrFrameWithoutMove.FovX, vrFrameWithoutMove.FovY, res.FrameMatrices );
	Scene.GenerateFrameSurfaceList( res.FrameMatrices, res.Surfaces );

	//------------------------------------------------------------------------------------------
	// calculate the controller pose from the most recent scene pose
	Vector3f pointerStart( 0.0f );
	Vector3f pointerEnd( 0.0f );

	// loop through all devices to update controller arm models and place the pointer for the dominant hand
	Matrix4f traceMat( res.FrameMatrices.CenterView.Inverted() );
	for ( int i = (int)InputDevices.size() - 1; i >= 0; --i )
	{
		ovrInputDeviceBase * device = InputDevices[i];
		if ( device == nullptr )
		{
			OVR_ASSERT( false );	// this should never happen!
			continue;
		}
		ovrDeviceID deviceID = device->GetDeviceID();		
		if ( deviceID == ovrDeviceIdType_Invalid )
		{
			OVR_ASSERT( deviceID != ovrDeviceIdType_Invalid );
			continue;
		}
		if ( device->GetType() == ovrControllerType_TrackedRemote )
		{		
			ovrInputDevice_TrackedRemote & trDevice = *static_cast< ovrInputDevice_TrackedRemote*>( device );

			ovrArmModel & 			armModel = trDevice.GetArmModel();
			OVR::ovrDrawSurface & 	controllerSurface = trDevice.GetControllerSurface();
			const ovrTracking &		tracking = trDevice.GetTracking();

			Array< ovrJoint > worldJoints = armModel.GetSkeleton().GetJoints();

			Posef remotePoseWithoutPosition( tracking.HeadPose.Pose );
			remotePoseWithoutPosition.Translation = Vector3f( 0.0f, 0.0f, 0.0f );

			Posef headPoseWithoutPosition( vrFrame.Tracking.HeadPose.Pose );
			headPoseWithoutPosition.Translation = Vector3f( 0.0f, 0.0f, 0.0f );

			Posef remotePose;
			armModel.Update( headPoseWithoutPosition, remotePoseWithoutPosition, trDevice.GetHand(),
					recenteredController, remotePose );

				Posef neckPose( Quatf(), Vector3f( 0.0f, vrFrame.EyeHeight, 0.0f ) );
				RenderBones( vrFrame, ParticleSystem, *SpriteAtlas, 0, RemoteBeamRenderer, *BeamAtlas, 0,
					neckPose, armModel.GetTransformedJoints(), trDevice.GetJointHandles() );

			Matrix4f mat = Matrix4f( tracking.HeadPose.Pose );

			controllerSurface.surface = &ControllerModelGear->Models[0].surfaces[0].surfaceDef;

			// TODO: The pitch offset should only be applied to the Gear VR controller but should be baked into the model.
			float controllerPitch = DegreeToRad( 15.0f );

#if defined( OVR_OS_ANDROID )
			if ( trDevice.GetTrackedRemoteCaps().ControllerCapabilities & ovrControllerCaps_ModelOculusGo )
			{
				controllerSurface.surface = &ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef;
				controllerPitch = 0.0f;
			}
#endif

			// TODO: bake the yaw rotation into the models -- this probably requires changing VrShell, too.
			const float controllerYaw = DegreeToRad( 180.0f );
			controllerSurface.modelMatrix = mat * Matrix4f::RotationY( controllerYaw ) * Matrix4f::RotationX( controllerPitch );


			// only do the trace for the user's dominant hand
			if ( trDevice.GetHand() == dominantHand )
			{
				traceMat = mat;
				pointerStart = traceMat.Transform( Vector3f( 0.0f ) );
				pointerEnd = traceMat.Transform( Vector3f( 0.0f, 0.0f, -10.0f ) );

				Vector3f const pointerDir = ( pointerEnd - pointerStart ).Normalized();
				HitTestResult hit = GuiSys->TestRayIntersection( pointerStart, pointerDir );
				LaserHit = hit.HitHandle.IsValid();
				if ( LaserHit )
				{
					pointerEnd = pointerStart + hit.RayDir * hit.t - pointerDir * 0.025f;//pointerDir * 0.15f;
				}
				else
				{	
					pointerEnd = pointerStart + pointerDir * 10.0f;
				}
			}
			// update ribbons
			if ( Ribbons[trDevice.GetHand()] != nullptr )
			{
				Ribbons[trDevice.GetHand()]->Update( res.FrameMatrices.CenterView,
					ovrMatrix4f_GetTranslation( mat ),
					vrFrame.DeltaSeconds );
			}
		}
	}
	//------------------------------------------------------------------------------------------

#if 0
	// Update GUI systems after the app frame, but before rendering anything.
	GuiSys->Frame( vrFrameWithoutMove, res.FrameMatrices.CenterView, traceMat );
	// Append GuiSys surfaces.
	GuiSys->AppendSurfaceList( res.FrameMatrices.CenterView, &res.Surfaces );
#endif

	res.FrameIndex = vrFrameWithoutMove.FrameNumber;
	res.DisplayTime = vrFrameWithoutMove.PredictedDisplayTimeInSeconds;
	res.SwapInterval = app->GetSwapInterval();

	res.FrameFlags = 0;
	res.LayerCount = 0;

	ovrLayerProjection2 & worldLayer = res.Layers[ res.LayerCount++ ].Projection;
	worldLayer = vrapi_DefaultLayerProjection2();

	worldLayer.HeadPose = vrFrameWithoutMove.Tracking.HeadPose;
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		worldLayer.Textures[eye].ColorSwapChain = vrFrameWithoutMove.ColorTextureSwapChain[eye];
		worldLayer.Textures[eye].SwapChainIndex = vrFrameWithoutMove.TextureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = vrFrameWithoutMove.TexCoordsFromTanAngles;
	}
	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	res.ClearColorBuffer = true;
	res.ClearDepthBuffer = false;
	res.ClearColor = Vector4f( 0.0f, 0.0f, 0.0f, 1.0f );	// solid alpha for underlay camera support

	//------------------------------------------------------------------------------------------
	// if there an active controller, draw the laser pointer at the dominant hand position
	if ( hasActiveController )
	{
		if ( !LaserPointerBeamHandle.IsValid() )
		{
			LaserPointerBeamHandle = RemoteBeamRenderer->AddBeam( vrFrame, *BeamAtlas, 0, 0.032f, pointerStart, pointerEnd, 
					LASER_COLOR, ovrBeamRenderer::LIFETIME_INFINITE );
			LOG_WITH_TAG( "MLBULaser", "AddBeam %i", LaserPointerBeamHandle.Get() );

			// Hide the gaze cursor when the remote laser pointer is active.
			GuiSys->GetGazeCursor().HideCursor();
		}
		else
		{
			//LOG_WITH_TAG( "MLBUBeam", "UpdateBeam %i", LaserPointerBeamHandle );
			RemoteBeamRenderer->UpdateBeam( vrFrame, LaserPointerBeamHandle, *BeamAtlas, 0, 0.032f, pointerStart, pointerEnd, LASER_COLOR );
			//Vector3f s = viewPos + viewUp * -0.125f + viewFwd * 1.3f;
			//Vector3f e = viewPos + viewUp * 0.125f + viewFwd * 1.3f;
			//RemoteBeamRenderer->UpdateBeam( vrFrame, LaserPointerBeamHandle, *BeamAtlas, 0, 0.032f, s, e, Vector4f( 1.0f ) );
		}

		if ( !LaserPointerParticleHandle.IsValid() )
		{
			if ( LaserHit )
			{
				LaserPointerParticleHandle = ParticleSystem->AddParticle( vrFrame, pointerEnd, 0.0f, Vector3f( 0.0f ), Vector3f( 0.0f ), 
						LASER_COLOR, ovrEaseFunc::NONE, 0.0f, 0.1f, 0.1f, 0 );
				LOG_WITH_TAG( "MLBULaser", "AddParticle %i", LaserPointerParticleHandle.Get() );
			}
		}
		else
		{
			if ( LaserHit )
			{
				ParticleSystem->UpdateParticle( vrFrame, LaserPointerParticleHandle, pointerEnd, 0.0f, Vector3f( 0.0f ), Vector3f( 0.0f ),
						LASER_COLOR, ovrEaseFunc::NONE, 0.0f, 0.1f, 0.1f, 0 );
			}
			else
			{
				ParticleSystem->RemoveParticle( LaserPointerParticleHandle );
				LaserPointerParticleHandle.Release();
			}
		}

//		ParticleSystem->AddParticle( vrFrame, pointerStart_NoWrist, 0.0f, Vector3f( 0.0f ), Vector3f( 0.0f ), 
//				Vector4f( 0.0f, 1.0f, 1.0f, 1.0f ), ovrEaseFunc::ALPHA_IN_OUT_LINEAR, 0.0f, 0.025f, 0.5f, 0 );
		ParticleSystem->AddParticle( vrFrame, pointerStart, 0.0f, Vector3f( 0.0f ), Vector3f( 0.0f ), 
				Vector4f( 0.0f, 1.0f, 0.0f, 0.5f ), ovrEaseFunc::ALPHA_IN_OUT_LINEAR, 0.0f, 0.025f, 0.05f, 0 );
	}
	else
	{
        ResetLaserPointer();
	}

	GuiSys->Frame( vrFrame, res.FrameMatrices.CenterView, traceMat );

	// since we don't delete any lines, we don't need to run its frame at all
	RemoteBeamRenderer->Frame( vrFrame, app->GetLastViewMatrix(), *BeamAtlas );
	ParticleSystem->Frame( vrFrame, *SpriteAtlas, res.FrameMatrices.CenterView );

	GuiSys->AppendSurfaceList( res.FrameMatrices.CenterView, &res.Surfaces );

	// add the controller model surfaces to the list of surfaces to render
	for ( int i = 0; i < (int)InputDevices.size(); ++i )
	{
		ovrInputDeviceBase * device = InputDevices[i];
		if ( device == nullptr )
		{
			OVR_ASSERT( false );	// this should never happen!
			continue;
		}
		if ( device->GetType() != ovrControllerType_TrackedRemote )
		{
			continue;
		}
		ovrInputDevice_TrackedRemote & trDevice = *static_cast< ovrInputDevice_TrackedRemote*>( device );
		if ( trDevice.GetSurface().surface != nullptr )
		{
			res.Surfaces.PushBack( trDevice.GetSurface() );
		}
		if ( Ribbons[trDevice.GetHand()] != nullptr )
		{
			Ribbons[trDevice.GetHand()]->Ribbon->GenerateSurfaceList( res.Surfaces );
		}
	}

	const Matrix4f projectionMatrix;
	ParticleSystem->RenderEyeView( res.FrameMatrices.CenterView, projectionMatrix, res.Surfaces );
	RemoteBeamRenderer->RenderEyeView( res.FrameMatrices.CenterView, projectionMatrix, res.Surfaces );

	return res;
}

void ovrVrController::ClearAndHideMenuItems()
{
	SetObjectColor( *GuiSys, Menu, "primary_input_trigger", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "primary_input_triggerana", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "primary_input_touchpad_click", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "primary_input_touch", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "primary_input_back", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "primary_input_grip", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "primary_input_gripana", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "primary_input_index_point", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "primary_input_thumb_up", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );

	SetObjectColor( *GuiSys, Menu, "secondary_input_trigger", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "secondary_input_triggerana", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "secondary_input_touchpad_click", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "secondary_input_touch", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "secondary_input_back", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "secondary_input_grip", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "secondary_input_gripana", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "secondary_input_index_point", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "secondary_input_thumb_up", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );

	SetObjectColor( *GuiSys, Menu, "tertiary_input_l1", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_l2", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_r1", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_r2", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_x", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_y", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_a", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_b", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_dpad", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_lstick", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_rstick", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );
	SetObjectColor( *GuiSys, Menu, "tertiary_input_back", Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) );

	SetObjectVisible( *GuiSys, Menu, "primary_input_trigger", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_triggerana", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_touchpad_click", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_touch", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_touch_pos", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_back", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_grip", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_gripana", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_touch_pos", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_button_caps", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_size_caps", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_range_caps", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_battery", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_hand", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_index_point", false );
	SetObjectVisible( *GuiSys, Menu, "primary_input_thumb_up", false );

	SetObjectVisible( *GuiSys, Menu, "secondary_input_trigger", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_triggerana", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_touchpad_click", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_touch", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_touch_pos", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_back", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_grip", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_gripana", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_touch_pos", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_button_caps", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_size_caps", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_range_caps", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_battery", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_hand", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_index_point", false );
	SetObjectVisible( *GuiSys, Menu, "secondary_input_thumb_up", false );

	SetObjectVisible( *GuiSys, Menu, "tertiary_input_l1", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_l2", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_r1", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_r2", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_x", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_y", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_a", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_b", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_dpad", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_lstick", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_rstick", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_back", false );
	SetObjectVisible( *GuiSys, Menu, "tertiary_input_header", false );
}

ovrResult ovrVrController::PopulateRemoteControllerInfo( ovrInputDevice_TrackedRemote & trDevice, bool recenteredController )
{
	ovrDeviceID deviceID = trDevice.GetDeviceID();

	const ovrArmModel::ovrHandedness controllerHand = trDevice.GetHand();

	ovrArmModel::ovrHandedness dominantHand = vrapi_GetSystemPropertyInt( app->GetJava(), VRAPI_SYS_PROP_DOMINANT_HAND ) == VRAPI_HAND_LEFT ? ovrArmModel::HAND_LEFT : ovrArmModel::HAND_RIGHT;

	ovrInputStateTrackedRemote remoteInputState;
	remoteInputState.Header.ControllerType = trDevice.GetType();

	ovrResult result;
	result = vrapi_GetCurrentInputState( app->GetOvrMobile(), deviceID, &remoteInputState.Header );

	if ( result != ovrSuccess )
	{
		LOG_WITH_TAG( "MLBUState", "ERROR %i getting remote input state!", result );
		OnDeviceDisconnected( deviceID );
		return result;
	}

	String headerObjectName;
	String triggerObjectName;
	String triggerAnalogObjectName;
	String gripObjectName;
	String gripAnalogObjectName;
	String touchpadClickObjectName;
	String touchObjectName;
	String touchPosObjectName;
	String backObjectName;
	String rangeObjectName;
	String sizeObjectName;
	String buttonCapsObjectName;
	String handObjectName;
	String batteryObjectName;
	String pointingObjectName;
	String thumbUpObjectName;
	
	if ( controllerHand == dominantHand )
	{
		headerObjectName = "primary_input_header";
		triggerObjectName = "primary_input_trigger";
		triggerAnalogObjectName = "primary_input_triggerana";
		gripObjectName = "primary_input_grip";
		gripAnalogObjectName = "primary_input_gripana";
		touchpadClickObjectName = "primary_input_touchpad_click";
		touchObjectName = "primary_input_touch";
		touchPosObjectName = "primary_input_touch_pos";
		backObjectName = "primary_input_back";
		rangeObjectName = "primary_input_range_caps";
		sizeObjectName = "primary_input_size_caps";
		buttonCapsObjectName = "primary_input_button_caps";
		handObjectName = "primary_input_hand";
		batteryObjectName = "primary_input_battery";
		pointingObjectName = "primary_input_index_point";
		thumbUpObjectName = "primary_input_thumb_up";
	}
	else
	{
		headerObjectName = "secondary_input_header";
		triggerObjectName = "secondary_input_trigger";
		triggerAnalogObjectName = "secondary_input_triggerana";
		gripObjectName = "secondary_input_grip";
		gripAnalogObjectName = "secondary_input_gripana";
		touchpadClickObjectName = "secondary_input_touchpad_click";
		touchObjectName = "secondary_input_touch";
		touchPosObjectName = "secondary_input_touch_pos";
		backObjectName = "secondary_input_back";
		rangeObjectName = "secondary_input_range_caps";
		sizeObjectName = "secondary_input_size_caps";
		buttonCapsObjectName = "secondary_input_button_caps";
		handObjectName = "secondary_input_hand";
		batteryObjectName = "secondary_input_battery";
		pointingObjectName = "secondary_input_index_point";
		thumbUpObjectName = "secondary_input_thumb_up";
	}

	String buttons;
	char temp[128];
	OVR_sprintf( temp, sizeof( temp ), "( %.2f, %.2f ) ",
		remoteInputState.TrackpadPosition.x,
		remoteInputState.TrackpadPosition.y );
	buttons += temp;

	const ovrInputTrackedRemoteCapabilities* inputTrackedRemoteCapabilities = reinterpret_cast<const ovrInputTrackedRemoteCapabilities*>( trDevice.GetCaps() );

#if defined( OVR_OS_ANDROID )
	if ( ( inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_ModelOculusGo ) != 0 )
	{
		SetObjectText( *GuiSys, Menu, headerObjectName.ToCStr(), "Oculus Go Controller" );
	}
	else
#endif
	{
		SetObjectText( *GuiSys, Menu, headerObjectName.ToCStr(), "Gear VR Controller" );
	}

	String buttonStr = "";
	if ( inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_A )
	{
		buttonStr += "TRIGGER ";
		SetObjectVisible( *GuiSys, Menu, triggerObjectName.ToCStr(), true );
		SetObjectVisible( *GuiSys, Menu, triggerAnalogObjectName.ToCStr(), true );
	}


	if ( inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_Enter )
	{
		buttonStr += "TP(Click) ";
		SetObjectVisible( *GuiSys, Menu, touchpadClickObjectName.ToCStr(), true );
	}

	if ( inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_Back )
	{
		buttonStr += "BACK ";
		SetObjectVisible( *GuiSys, Menu, backObjectName.ToCStr(), true );
	}

	SetObjectVisible( *GuiSys, Menu, rangeObjectName.ToCStr(), true );
	SetObjectText( *GuiSys, Menu, rangeObjectName.ToCStr(), "Touch Range: ( %i, %i )",
		( int )inputTrackedRemoteCapabilities->TrackpadMaxX, ( int )inputTrackedRemoteCapabilities->TrackpadMaxY );
	SetObjectVisible( *GuiSys, Menu, sizeObjectName.ToCStr(), true );
	SetObjectText( *GuiSys, Menu, sizeObjectName.ToCStr(), "Touch Size: ( %.0f, %.0f )",
		inputTrackedRemoteCapabilities->TrackpadSizeX, inputTrackedRemoteCapabilities->TrackpadSizeY );
	SetObjectVisible( *GuiSys, Menu, buttonCapsObjectName.ToCStr(), true );
	SetObjectText( *GuiSys, Menu, buttonCapsObjectName.ToCStr(), "Buttons: %s",
		buttonStr.ToCStr() );

	if ( remoteInputState.Buttons & ovrButton_A
		&& remoteInputState.Buttons & ovrButton_Enter )
	{
		HighLightMask.x = 1.0f;
		HighLightMask.y = 1.0f;
		HighLightMask.z = 1.0f;
		HighLightMask.w = 1.0f;
	}
	else
	{
		HighLightMask = Vector4f::ZERO;
	}

	if ( remoteInputState.Buttons & ovrButton_A )
	{
		buttons += "A ";
		SetObjectColor( *GuiSys, Menu, triggerObjectName.ToCStr(),
			Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
	}

	if ( remoteInputState.Buttons & ovrButton_Enter )
	{
		buttons += "TRACKPAD ";
		SetObjectColor( *GuiSys, Menu, touchpadClickObjectName.ToCStr(),
			Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
	}

	if ( remoteInputState.Buttons & ovrButton_Back )
	{
		buttons += "BACK ";
		SetObjectColor( *GuiSys, Menu, backObjectName.ToCStr(),
			Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
	}

	SetObjectVisible( *GuiSys, Menu, touchObjectName.ToCStr(), true );
	if ( remoteInputState.TrackpadStatus )
	{
		SetObjectColor( *GuiSys, Menu, touchObjectName.ToCStr(),
			Vector4f( 1.0f, 0.25f, 0.25f, 1.0f ) );
	}

	char const *handStr =
		controllerHand == ovrArmModel::HAND_LEFT ? "Left" : "Right";
	SetObjectVisible( *GuiSys, Menu, handObjectName.ToCStr(), true );
	SetObjectText( *GuiSys, Menu, handObjectName.ToCStr(), "Hand: %s", handStr );

	SetObjectVisible( *GuiSys, Menu, batteryObjectName.ToCStr(), true );
	SetObjectText( *GuiSys, Menu, batteryObjectName.ToCStr(), "Battery: %d",
		remoteInputState.BatteryPercentRemaining );

	static Vector2f minTrackpad( FLT_MAX );
	static Vector2f maxTrackpad( -FLT_MAX );
	Vector2f mm( 0.0f );
	TrackpadStats( remoteInputState.TrackpadPosition,
		Vector2f( trDevice.GetTrackedRemoteCaps().TrackpadMaxX,
			trDevice.GetTrackedRemoteCaps().TrackpadMaxY ),
		Vector2f( trDevice.GetTrackedRemoteCaps().TrackpadSizeX,
			trDevice.GetTrackedRemoteCaps().TrackpadSizeY ),
		minTrackpad, maxTrackpad, mm );
	SetObjectVisible( *GuiSys, Menu, touchPosObjectName.ToCStr(), true );
	SetObjectText( *GuiSys, Menu, touchPosObjectName.ToCStr(),
		"Pos( %.2f, %.2f ) Min( %.2f, %.2f ) Max( %.2f, %.2f )",
		remoteInputState.TrackpadPosition.x,
		remoteInputState.TrackpadPosition.y,
		minTrackpad.x, minTrackpad.y, maxTrackpad.x, maxTrackpad.y );

	if ( remoteInputState.RecenterCount != trDevice.GetLastRecenterCount() )
	{
		recenteredController = true;
		LOG_WITH_TAG( "MLBUState", "**RECENTERED** (%i != %i )",
			( int )remoteInputState.RecenterCount,
			( int )trDevice.GetLastRecenterCount() );
		trDevice.SetLastRecenterCount( remoteInputState.RecenterCount );
	}
	

	return result;
}

//---------------------------------------------------------------------------------------------------
// Input device management
//---------------------------------------------------------------------------------------------------

//==============================
// ovrVrController::FindInputDevice
int ovrVrController::FindInputDevice( const ovrDeviceID deviceID ) const
{
	for ( int i = 0; i < (int)InputDevices.size(); ++i )
	{
		if ( InputDevices[i]->GetDeviceID() == deviceID )
		{
			return i;
		}
	}
	return -1;
}

//==============================
// ovrVrController::RemoveDevice
void ovrVrController::RemoveDevice( const ovrDeviceID deviceID )
{
	int index = FindInputDevice( deviceID );
	if ( index < 0 )
	{
		return;
	}
	ovrInputDeviceBase * device = InputDevices[index];
	delete device;
	InputDevices[index] = InputDevices.back();
	InputDevices[InputDevices.size() - 1] = nullptr;
	InputDevices.pop_back();
}

//==============================
// ovrVrController::IsDeviceTracked
bool ovrVrController::IsDeviceTracked( const ovrDeviceID deviceID ) const
{
	return FindInputDevice( deviceID ) >= 0;
}

//==============================
// ovrVrController::EnumerateInputDevices
void ovrVrController::EnumerateInputDevices()
{
	for ( uint32_t deviceIndex = 0; ; deviceIndex++ )
	{
		ovrInputCapabilityHeader curCaps;

		if ( vrapi_EnumerateInputDevices( app->GetOvrMobile(), deviceIndex, &curCaps ) < 0 )
		{
			//LOG_WITH_TAG( "Input", "No more devices!" );
			break;	// no more devices
		}

		if ( !IsDeviceTracked( curCaps.DeviceID ) )
		{
			LOG_WITH_TAG( "Input", "     tracked" );
			OnDeviceConnected( curCaps );
		}
	}
}

//==============================
// ovrVrController::OnDeviceConnected
void ovrVrController::OnDeviceConnected( const ovrInputCapabilityHeader & capsHeader )
{
	ovrInputDeviceBase * device = nullptr;
	ovrResult result = ovrError_NotInitialized;
	switch( capsHeader.Type )
	{
		case ovrControllerType_Headset:
			{
				LOG_WITH_TAG( "MLBUConnect", "Headset connected, ID = %u", capsHeader.DeviceID );
				ovrInputHeadsetCapabilities headsetCapabilities;
				headsetCapabilities.Header = capsHeader;
				result = vrapi_GetInputDeviceCapabilities( app->GetOvrMobile(), &headsetCapabilities.Header );
				if ( result == ovrSuccess )
				{
					device = ovrInputDevice_Headset::Create( *app, *GuiSys, *Menu, headsetCapabilities );
				}
			}
			break;
		case ovrControllerType_TrackedRemote:
			{
				LOG_WITH_TAG( "MLBUConnect", "Controller connected, ID = %u", capsHeader.DeviceID );

				ovrInputTrackedRemoteCapabilities remoteCapabilities;
				remoteCapabilities.Header = capsHeader;
				result = vrapi_GetInputDeviceCapabilities( app->GetOvrMobile(), &remoteCapabilities.Header );
				if ( result == ovrSuccess )
				{
					device = ovrInputDevice_TrackedRemote::Create( *app, *GuiSys, *Menu, remoteCapabilities );
					// reflect the device type in the UI
					VRMenuObject * header = Menu->ObjectForName( *GuiSys, "primary_input_header" );
					if ( header != nullptr )
					{
#if defined( OVR_OS_ANDROID )
						if ( ( remoteCapabilities.ControllerCapabilities & ovrControllerCaps_ModelOculusGo ) != 0 )
						{
							header->SetText( "Oculus Go old Controller" );
						}
						else
#endif
						{
							header->SetText( "Gear VR old  Controller" );
						}
					}
				}
				break;
		}
			
		case ovrControllerType_Gamepad:
			{
				LOG_WITH_TAG( "MLBUConnect", "Gamepad connected, ID = %u", capsHeader.DeviceID );
				ovrInputGamepadCapabilities gamepadCapabilities;
				gamepadCapabilities.Header = capsHeader;
				result = vrapi_GetInputDeviceCapabilities( app->GetOvrMobile(), &gamepadCapabilities.Header );
				if ( result == ovrSuccess )
				{
					device = ovrInputDevice_Gamepad::Create( *app, *GuiSys, *Menu, gamepadCapabilities );
				}
			}
			break;
			
		default:
			LOG( "Unknown device connected!");
			OVR_ASSERT( false );
			return;
	}

	if ( result != ovrSuccess )
	{
		LOG_WITH_TAG( "MLBUConnect", "vrapi_GetInputDeviceCapabilities: Error %i", result );
	}
	if ( device != nullptr )
	{
		LOG_WITH_TAG( "MLBUConnect", "Added device '%s', id = %u", device->GetName(), capsHeader.DeviceID );
		InputDevices.push_back( device );
	}
	else
	{
		LOG_WITH_TAG( "MLBUConnect", "Device creation failed for id = %u", capsHeader.DeviceID );
	}
}

//==============================
// ovrVrController::OnDeviceDisconnected
void ovrVrController::OnDeviceDisconnected( const ovrDeviceID deviceID )
{
	LOG_WITH_TAG( "MLBUConnect", "Controller disconnected, ID = %i", deviceID );
    int deviceIndex = FindInputDevice( deviceID );
    if ( deviceIndex >= 0 )
    {
        ovrInputDeviceBase * device = InputDevices[deviceIndex];
        if ( device != nullptr && device->GetType() == ovrControllerType_TrackedRemote )
        {
            ovrInputDevice_TrackedRemote & trDevice = *static_cast< ovrInputDevice_TrackedRemote* >( device );
            ResetBones( ParticleSystem, RemoteBeamRenderer, trDevice.GetJointHandles() );
        }
    }
	RemoveDevice( deviceID );
}

//==============================
// ovrInputDevice_Gamepad::Create
ovrInputDeviceBase * ovrInputDevice_Gamepad::Create( App & app, OvrGuiSys & guiSys, VRMenu & menu,
	const ovrInputGamepadCapabilities & gamepadCapabilities )
{
	ovrInputDevice_Gamepad * device = new ovrInputDevice_Gamepad( gamepadCapabilities );

	LOG_WITH_TAG( "MLBUConnect", "Gamepad" );

	return device;
}

//==============================
// ovrInputDevice_Headset::Create
ovrInputDeviceBase * ovrInputDevice_Headset::Create( App & app, OvrGuiSys & guiSys, VRMenu & menu, 
		const ovrInputHeadsetCapabilities & headsetCapabilities )
{
	ovrInputDevice_Headset * device = new ovrInputDevice_Headset( headsetCapabilities );

	LOG_WITH_TAG( "MLBUConnect", "Headset caps: Button %x, Controller %x, MaxX %d, MaxY %d, SizeX %f SizeY %f",
			headsetCapabilities.ButtonCapabilities,
			headsetCapabilities.ControllerCapabilities,
			headsetCapabilities.TrackpadMaxX, headsetCapabilities.TrackpadMaxY,
			headsetCapabilities.TrackpadSizeX, headsetCapabilities.TrackpadSizeY );

	return device;
}

//==============================
// ovrInputDevice_TrackedRemote::Create
ovrInputDeviceBase * ovrInputDevice_TrackedRemote::Create( App & app, OvrGuiSys & guiSys, VRMenu & menu, 
		const ovrInputTrackedRemoteCapabilities & remoteCapabilities )
{
	LOG_WITH_TAG( "MLBUConnect", "ovrInputDevice_TrackedRemote::Create" );

	ovrInputStateTrackedRemote remoteInputState;
	remoteInputState.Header.ControllerType = remoteCapabilities.Header.Type;
	ovrResult result = vrapi_GetCurrentInputState( app.GetOvrMobile(), remoteCapabilities.Header.DeviceID, &remoteInputState.Header );
	if ( result == ovrSuccess )
	{
		ovrInputDevice_TrackedRemote * device = new ovrInputDevice_TrackedRemote( remoteCapabilities, remoteInputState.RecenterCount );
	
		ovrArmModel::ovrHandedness controllerHand = ovrArmModel::HAND_RIGHT;
		if ( ( remoteCapabilities.ControllerCapabilities & ovrControllerCaps_LeftHand ) != 0 )
		{
			controllerHand = ovrArmModel::HAND_LEFT;
		}

		char const * handStr = controllerHand == ovrArmModel::HAND_LEFT ? "left" : "right";
		LOG_WITH_TAG( "MLBUConnect", "Controller caps: hand = %s, Button %x, Controller %x, MaxX %d, MaxY %d, SizeX %f SizeY %f",
				handStr, remoteCapabilities.ButtonCapabilities,
				remoteCapabilities.ControllerCapabilities,
				remoteCapabilities.TrackpadMaxX, remoteCapabilities.TrackpadMaxY,
				remoteCapabilities.TrackpadSizeX, remoteCapabilities.TrackpadSizeY );

		device->ArmModel.InitSkeleton();
		device->JointHandles.Resize( device->ArmModel.GetSkeleton().GetJoints().GetSizeI() );


		return device;
	}
	else
	{
		LOG_WITH_TAG( "MLBUConnect", "vrapi_GetCurrentInputState: Error %i", result );
	}

	return nullptr;
}


} // namespace OVR
