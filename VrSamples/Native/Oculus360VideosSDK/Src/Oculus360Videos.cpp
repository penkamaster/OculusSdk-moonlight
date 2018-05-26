/************************************************************************************

Filename    :   Oculus360Videos.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include <sys/stat.h>
#include <sys/types.h>
#include "OVR_Input.h"

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_String_Utils.h"
#include "OVR_GlUtils.h"

#include "GlTexture.h"
#include "GazeCursor.h"
#include "App.h"
#include "Oculus360Videos.h"
#include "GuiSys.h"
#include "ImageData.h"
#include "PackageFiles.h"
#include "Fader.h"
#include "VrCommon.h"

#include "VideoBrowser.h"
#include "VideoMenu.h"
#include "OVR_Locale.h"
#include "PathUtils.h"

#include "VideosMetaData.h"

#if defined( OVR_OS_ANDROID )
#include <dirent.h>
#include <jni.h>
#endif

static bool	RetailMode = false;

static const char * videosDirectory = "Oculus/360Videos/";
static const char * videosLabel = "@string/app_name";

static jclass GlobalActivityClass;

#if defined( OVR_OS_ANDROID )
extern "C" {

long Java_com_oculus_oculus360videossdk_MainActivity_nativeSetAppInterface( JNIEnv *jni, jclass clazz, jobject activity,
		jstring fromPackageName, jstring commandString, jstring uriString )
{
	// This is called by the java UI thread.

	GlobalActivityClass = (jclass)jni->NewGlobalRef( clazz );

	LOG( "nativeSetAppInterface");
	return (new OVR::Oculus360Videos())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

void Java_com_oculus_oculus360videossdk_MainActivity_nativeFrameAvailable( JNIEnv *jni, jclass clazz, jlong interfacePtr )
{
	OVR::Oculus360Videos * videos = static_cast< OVR::Oculus360Videos * >( ( ( OVR::App * )interfacePtr )->GetAppInterface() );
	videos->SetFrameAvailable( true );
}

jobject Java_com_oculus_oculus360videossdk_MainActivity_nativePrepareNewVideo( JNIEnv *jni, jclass clazz, jlong interfacePtr )
{
	// set up a message queue to get the return message
	// TODO: make a class that encapsulates this work
	OVR::ovrMessageQueue result( 1 );
	OVR::Oculus360Videos * videos = static_cast< OVR::Oculus360Videos * >( ( ( OVR::App * )interfacePtr )->GetAppInterface() );
	videos->GetMessageQueue().PostPrintf( "newVideo %p", &result );

	result.SleepUntilMessage();
	const char * msg = result.GetNextMessage();
	jobject	texobj;
	sscanf( msg, "surfaceTexture %p", &texobj );
	free( ( void * )msg );

	return texobj;
}

void Java_com_oculus_oculus360videossdk_MainActivity_nativeSetVideoSize( JNIEnv *jni, jclass clazz, jlong interfacePtr, int width, int height )
{
	LOG( "nativeSetVideoSizes: width=%i height=%i", width, height );

	OVR::Oculus360Videos * videos = static_cast< OVR::Oculus360Videos * >( ( ( OVR::App * )interfacePtr )->GetAppInterface() );
	videos->GetMessageQueue().PostPrintf( "video %i %i", width, height );
}

void Java_com_oculus_oculus360videossdk_MainActivity_nativeVideoCompletion( JNIEnv *jni, jclass clazz, jlong interfacePtr )
{
	LOG( "nativeVideoCompletion" );

	OVR::Oculus360Videos * videos = static_cast< OVR::Oculus360Videos * >( ( ( OVR::App * )interfacePtr )->GetAppInterface() );
	videos->GetMessageQueue().PostPrintf( "completion" );
}

void Java_com_oculus_oculus360videossdk_MainActivity_nativeVideoStartError( JNIEnv *jni, jclass clazz, jlong interfacePtr )
{
	LOG( "nativeVideoStartError" );

	OVR::Oculus360Videos * videos = static_cast< OVR::Oculus360Videos * >( ( ( OVR::App * )interfacePtr )->GetAppInterface() );
	videos->GetMessageQueue().PostPrintf( "startError" );
}

} // extern "C"

#endif

namespace OVR
{
static const char * ImageExternalDirectives =
#if defined( OVR_OS_ANDROID )
	"#extension GL_OES_EGL_image_external : enable\n"
	"#extension GL_OES_EGL_image_external_essl3 : enable\n";
#else
	"";
#endif

static const char * PanoramaVertexShaderSrc =
	"uniform highp mat4 Texm[NUM_VIEWS];\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
	"varying  highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = TransformVertex( Position );\n"
	"   oTexCoord = vec2( Texm[VIEW_ID] * vec4( TexCoord, 0, 1 ) );\n"
	"}\n";

static const char * PanoramaFragmentShaderSrc =
	"uniform samplerExternalOES Texture0;\n"
	"uniform lowp vec4 UniformColor;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = UniformColor * texture2D( Texture0, oTexCoord );\n"
	"}\n";

static const char * FadedPanoramaFragmentShaderSrc =
	"uniform samplerExternalOES Texture0;\n"
	"uniform sampler2D Texture1;\n"
	"uniform lowp vec4 UniformColor;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"	lowp vec4 staticColor = texture2D( Texture1, oTexCoord );\n"
	"	lowp vec4 movieColor = texture2D( Texture0, oTexCoord );\n"
	"	gl_FragColor = UniformColor * mix( movieColor, staticColor, staticColor.w );\n"
	"}\n";

//==============================================================
// ovrGuiSoundEffectPlayer
class ovrGuiSoundEffectPlayer : public OvrGuiSys::SoundEffectPlayer
{
public:
	ovrGuiSoundEffectPlayer( ovrSoundEffectContext & context )
		: SoundEffectContext( context )
	{
	}

	virtual bool Has( const char * name ) const OVR_OVERRIDE { return SoundEffectContext.GetMapping().HasSound( name ); }
	virtual void Play( const char * name ) OVR_OVERRIDE { SoundEffectContext.Play( name ); }

private:
	ovrGuiSoundEffectPlayer &	operator = ( ovrGuiSoundEffectPlayer & );
private:
	ovrSoundEffectContext & SoundEffectContext;
};

Oculus360Videos::Oculus360Videos() :
	  MainActivityClass( GlobalActivityClass )
	, MessageQueue( 100 )
	, SoundEffectContext( NULL )
	, SoundEffectPlayer( NULL )
	, GuiSys( OvrGuiSys::Create() )
	, Locale( NULL )
	, GlobeProgramMatrices()
	, BackgroundScene( NULL )
	, MetaData( NULL )
	, Browser( NULL )
	, VideoMenu( NULL )
	, ActiveVideo( NULL )
	, MenuState( MENU_NONE )
	, Fader( 1.0f )
	, FadeOutRate( 1.0f / 0.5f )
	, VideoMenuVisibleTime( 5.0f )
	, CurrentFadeRate( FadeOutRate )
	, CurrentFadeLevel( 1.0f )
	, VideoMenuTimeLeft( 0.0f )
	, UseSrgb( false )
	, MovieTexture( NULL )
	, CurrentVideoWidth( 0 )
	, CurrentVideoHeight( 480 )
	, BackgroundWidth( 0 )
	, BackgroundHeight( 0 )
	, FrameAvailable( false )
	, VideoPaused( false )
	, HmdMounted( true )
{
}

Oculus360Videos::~Oculus360Videos()
{
	LOG( "--------------- ~Oculus360Videos() ---------------" );

	delete SoundEffectPlayer;
	SoundEffectPlayer = NULL;

	delete SoundEffectContext;
	SoundEffectContext = NULL;

	if ( BackgroundScene != NULL )
	{
		delete BackgroundScene;
		BackgroundScene = NULL;
	}

	if ( MetaData != NULL )
	{
		delete MetaData;
		MetaData = NULL;
	}

	GlobeSurfaceDef.geo.Free();

	FreeTexture( BackgroundTexId );

	if ( MovieTexture != NULL )
	{
		delete MovieTexture;
		MovieTexture = NULL;
	}

	DeleteProgram( SingleColorTextureProgram );
	GlProgram::Free( PanoramaProgram );
	GlProgram::Free( FadedPanoramaProgram );

	OvrGuiSys::Destroy( GuiSys );
}

void Oculus360Videos::Configure( ovrSettings & settings )
{
	// We need very little CPU for pano browsing, but a fair amount of GPU.
	settings.CpuLevel = 1;
	settings.GpuLevel = 0;

	// We could disable the srgb convert on the FBO. but this is easier
	settings.EyeBufferParms.colorFormat = UseSrgb ? COLOR_8888_sRGB : COLOR_8888;
	settings.EyeBufferParms.depthFormat = DEPTH_16;
	// All geometry is blended, so save power with no MSAA
	settings.EyeBufferParms.multisamples = 1;

	settings.TrackingTransform = VRAPI_TRACKING_TRANSFORM_SYSTEM_CENTER_EYE_LEVEL;

	settings.RenderMode = RENDERMODE_MULTIVIEW;
}

void Oculus360Videos::EnteredVrMode( const ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI )
{
	OVR_UNUSED( intentFromPackage );
	OVR_UNUSED( intentJSON );
	OVR_UNUSED( intentURI );

	if ( intentType == INTENT_LAUNCH )
	{
		// This is called by the VR thread, not the java UI thread.
		LOG( "--------------- Oculus360Videos OneTimeInit ---------------" );

		const ovrJava * java = app->GetJava();
		SoundEffectContext = new ovrSoundEffectContext( *java->Env, java->ActivityObject );
		SoundEffectContext->Initialize( &app->GetFileSys() );
		SoundEffectPlayer = new ovrGuiSoundEffectPlayer( *SoundEffectContext );

		Locale = ovrLocale::Create( *java->Env, java->ActivityObject, "default" );

		String fontName;
		GetLocale().GetString( "@string/font_name", "efigs.fnt", fontName );
		GuiSys->Init( this->app, *SoundEffectPlayer, fontName.ToCStr(), &app->GetDebugLines() );

		GuiSys->GetGazeCursor().ShowCursor();

		RetailMode = FileExists( "/sdcard/RetailMedia" );

		SingleColorTextureProgram = BuildProgram(
				"attribute highp vec4 Position;\n"
				"attribute highp vec2 TexCoord;\n"
				"varying highp vec2 oTexCoord;\n"
				"void main()\n"
				"{\n"
				"   gl_Position = TransformVertex( Position );\n"
				"   oTexCoord = TexCoord;\n"
				"}\n"
			,
				"uniform sampler2D Texture0;\n"
				"uniform lowp vec4 UniformColor;\n"
				"varying highp vec2 oTexCoord;\n"
				"void main()\n"
				"{\n"
				"   gl_FragColor = UniformColor * texture2D( Texture0, oTexCoord );\n"
				"}\n"
			);

		static ovrProgramParm PanoramaUniformParms[] =
		{
			{ "Texm",			ovrProgramParmType::FLOAT_MATRIX4	},
			{ "UniformColor",	ovrProgramParmType::FLOAT_VECTOR4	},
			{ "Texture0",		ovrProgramParmType::TEXTURE_SAMPLED	},
		};

		static ovrProgramParm FadedPanoramaUniformParms[] =
		{
			{ "Texm",			ovrProgramParmType::FLOAT_MATRIX4	},
			{ "UniformColor",	ovrProgramParmType::FLOAT_VECTOR4	},
			{ "Texture0",		ovrProgramParmType::TEXTURE_SAMPLED	},
			{ "Texture1",		ovrProgramParmType::TEXTURE_SAMPLED	},
		};

		PanoramaProgram = GlProgram::Build( NULL, PanoramaVertexShaderSrc, ImageExternalDirectives, PanoramaFragmentShaderSrc,
											PanoramaUniformParms, sizeof( PanoramaUniformParms ) / sizeof( ovrProgramParm ) );

		FadedPanoramaProgram = GlProgram::Build( NULL, PanoramaVertexShaderSrc, ImageExternalDirectives, FadedPanoramaFragmentShaderSrc,
											FadedPanoramaUniformParms, sizeof( FadedPanoramaUniformParms ) / sizeof( ovrProgramParm ) );

		LOG( "Creating Globe" );
		GlobeSurfaceDef.surfaceName = "Globe";
		GlobeSurfaceDef.geo = BuildGlobe();
		GlobeSurfaceDef.graphicsCommand.Program = PanoramaProgram;
		GlobeSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
		GlobeSurfaceDef.graphicsCommand.GpuState.cullEnable = false;

		const char * launchPano = NULL;
		if ( ( NULL != launchPano ) && launchPano[ 0 ] )
		{
			BackgroundTexId = LoadTextureFromBuffer( launchPano, MemBufferFile( launchPano ),
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ) | TEXTUREFLAG_USE_SRGB, BackgroundWidth, BackgroundHeight );
		}

		// always fall back to valid background
		if ( BackgroundTexId == 0 )
		{
			BackgroundTexId = LoadTextureFromApplicationPackage( "assets/background.jpg",
				TextureFlags_t( TEXTUREFLAG_USE_SRGB ), BackgroundWidth, BackgroundHeight );
		}

		MaterialParms materialParms;
		materialParms.UseSrgbTextureFormats = UseSrgb;

		BackgroundScene = LoadModelFileFromApplicationPackage( "assets/stars.ovrscene",
			Scene.GetDefaultGLPrograms(),
			materialParms );

		if ( BackgroundScene != NULL )
		{
			Scene.SetWorldModel( *BackgroundScene );
		}

		// Load up meta data from videos directory
		MetaData = new OvrVideosMetaData();
		if ( MetaData == NULL )
		{
			FAIL( "Oculus360Photos::OneTimeInit failed to create MetaData" );
		}

		const OvrStoragePaths & storagePaths = app->GetStoragePaths();
		storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
		storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );
		storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
		storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );

		OvrMetaDataFileExtensions fileExtensions;
		fileExtensions.GoodExtensions.PushBack( ".mp4" );
		fileExtensions.GoodExtensions.PushBack( ".m4v" );
		fileExtensions.GoodExtensions.PushBack( ".3gp" );
		fileExtensions.GoodExtensions.PushBack( ".3g2" );
		fileExtensions.GoodExtensions.PushBack( ".ts" );
		fileExtensions.GoodExtensions.PushBack( ".webm" );
		fileExtensions.GoodExtensions.PushBack( ".mkv" );
		fileExtensions.GoodExtensions.PushBack( ".wmv" );
		fileExtensions.GoodExtensions.PushBack( ".asf" );
		fileExtensions.GoodExtensions.PushBack( ".avi" );
		fileExtensions.GoodExtensions.PushBack( ".flv" );

		MetaData->InitFromDirectory( videosDirectory, SearchPaths, fileExtensions );

		String localizedAppName;
		GetLocale().GetString( videosLabel, videosLabel, localizedAppName );
		MetaData->RenameCategory( ExtractFileBase( videosDirectory ).ToCStr(), localizedAppName.ToCStr() );

		// Start building the VideoMenu
		VideoMenu = ( OvrVideoMenu * )GuiSys->GetMenu( OvrVideoMenu::MENU_NAME );
		if ( VideoMenu == NULL )
		{
			VideoMenu = OvrVideoMenu::Create( *GuiSys, *MetaData, 2.0f );
			OVR_ASSERT( VideoMenu );

			GuiSys->AddMenu( VideoMenu );
		}

		VideoMenu->SetFlags( VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ) | VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP );

		// Start building the FolderView
		Browser = (VideoBrowser *)GuiSys->GetMenu( OvrFolderBrowser::MENU_NAME );
		if ( Browser == NULL )
		{
			Browser = VideoBrowser::Create(
				*this,
				*GuiSys,
				*MetaData,
				256, 20.0f,
				256, 200.0f,
				7,
				5.4f );
			OVR_ASSERT( Browser );

			GuiSys->AddMenu( Browser );
		}

		Browser->SetFlags( VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ) | VRMENU_FLAG_BACK_KEY_EXITS_APP );
		Browser->SetFolderTitleSpacingScale( 0.37f );
		Browser->SetPanelTextSpacingScale( 0.34f );
		Browser->SetScrollBarSpacingScale( 0.9f );
		Browser->SetScrollBarRadiusScale( 1.0f );

		Browser->OneTimeInit( *GuiSys );
		Browser->BuildDirtyMenu( *GuiSys, *MetaData );

		SetMenuState( MENU_BROWSER );
	}
	else if ( intentType == INTENT_NEW )
	{
	}

	LOG( "Oculus360Videos::EnteredVrMode" );
	Browser->SetMenuPose( Posef() );
	VideoMenu->SetMenuPose( Posef() );
}

void Oculus360Videos::LeavingVrMode()
{
	LOG( "Oculus360Videos::LeavingVrMode" );
	if ( MenuState == MENU_VIDEO_PLAYING )
	{
		SetMenuState( MENU_VIDEO_PAUSE );
	}
}

bool Oculus360Videos::OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType )
{
	if ( GuiSys->OnKeyEvent( keyCode, repeatCount, eventType ) )
	{
		return true;
	}

	if ( ( ( keyCode == OVR_KEY_BACK ) && ( eventType == KEY_EVENT_SHORT_PRESS ) ) ||
		 ( ( keyCode == OVR_KEY_BUTTON_B ) && ( eventType == KEY_EVENT_UP ) ) )
	{
		if ( MenuState == MENU_VIDEO_LOADING )
		{
			return true;
		}

		if ( ActiveVideo )
		{
			SetMenuState( MENU_BROWSER );
			return true;	// consume the key
		}
		// if no video is playing (either paused or stopped), let VrLib handle the back key
	}
	else if ( keyCode == OVR_KEY_P && eventType == KEY_EVENT_DOWN )
	{
		PauseVideo( true );
	}

	return false;
}

void Oculus360Videos::Command( const char * msg )
{
	// Always include the space in MatchesHead to prevent problems
	// with commands with matching prefixes.

	if ( MatchesHead( "newVideo ", msg ) )
	{
		delete MovieTexture;
		MovieTexture = new SurfaceTexture( app->GetJava()->Env );
		LOG( "RC_NEW_VIDEO texId %i", MovieTexture->GetTextureId() );

		ovrMessageQueue * receiver;
		sscanf( msg, "newVideo %p", &receiver );

		receiver->PostPrintf( "surfaceTexture %p", MovieTexture->GetJavaObject() );

		// don't draw the screen until we have the new size
		CurrentVideoWidth = 0;

		return;
	}
	else if ( MatchesHead( "completion", msg ) ) // video complete, return to menu
	{
		SetMenuState( MENU_BROWSER );
		return;
	}
	else if ( MatchesHead( "video ", msg ) )
	{
		sscanf( msg, "video %i %i", &CurrentVideoWidth, &CurrentVideoHeight );

		if ( MenuState != MENU_VIDEO_PLAYING ) // If video is already being played dont change the state to video ready
		{
			SetMenuState( MENU_VIDEO_READY );
		}

		return;
	}
	else if ( MatchesHead( "startError", msg ) )
	{
		// FIXME: this needs to do some parameter magic to fix xliff tags
		String message;
		GetLocale().GetString( "@string/playback_failed", "@string/playback_failed", message );
		String fileName = ExtractFile( ActiveVideo->Url );
		message = ovrLocale::GetXliffFormattedString( message.ToCStr(), fileName.ToCStr() );

		GuiSys->GetDefaultFont().WordWrapText( message, 1.0f );
		GuiSys->ShowInfoText( 4.5f, message.ToCStr() );
		SetMenuState( MENU_BROWSER );
		return;
	}
}

Matrix4f Oculus360Videos::TexmForVideo( const int eye )
{
	if ( strstr( VideoName.ToCStr(), "_TB.mp4" ) )
	{	// top / bottom stereo panorama
		return eye ?
			Matrix4f(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.5f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f )
			:
			Matrix4f(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f );
	}
	if ( strstr( VideoName.ToCStr(), "_BT.mp4" ) )
	{	// top / bottom stereo panorama
		return ( !eye ) ?
			Matrix4f(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.5f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f )
			:
			Matrix4f(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f );
	}
	if ( strstr( VideoName.ToCStr(), "_LR.mp4" ) )
	{	// left / right stereo panorama
		return eye ?
			Matrix4f(
				0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f )
			:
			Matrix4f(
				0.5f, 0.0f, 0.0f, 0.5f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f );
	}
	if ( strstr( VideoName.ToCStr(), "_RL.mp4" ) )
	{	// left / right stereo panorama
		return ( !eye ) ?
			Matrix4f(
				0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f )
			:
			Matrix4f(
				0.5f, 0.0f, 0.0f, 0.5f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f );
	}

	// default to top / bottom stereo
	if ( CurrentVideoWidth == CurrentVideoHeight )
	{	// top / bottom stereo panorama
		return eye ?
			Matrix4f(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.5f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f )
			:
			Matrix4f(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f );

		// We may want to support swapping top/bottom
	}
	return Matrix4f::Identity();
}

Matrix4f Oculus360Videos::TexmForBackground( const int eye )
{
	if ( BackgroundWidth == BackgroundHeight )
	{	// top / bottom stereo panorama
		return eye ?
			Matrix4f(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.5f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f )
			:
			Matrix4f(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f );

		// We may want to support swapping top/bottom
	}
	return Matrix4f::Identity();
}

bool Oculus360Videos::IsVideoPlaying() const
{
#if defined( OVR_OS_ANDROID )
	jmethodID methodId = app->GetJava()->Env->GetMethodID( MainActivityClass, "isPlaying", "()Z" );
	if ( !methodId )
	{
		LOG( "Couldn't find isPlaying methodID" );
		return false;
	}

	bool isPlaying = app->GetJava()->Env->CallBooleanMethod( app->GetJava()->ActivityObject, methodId );
	return isPlaying;
#else
	return false;
#endif
}

void Oculus360Videos::PauseVideo( bool const force )
{
	LOG( "PauseVideo()" );

	OVR_UNUSED( force );

#if defined( OVR_OS_ANDROID )
	jmethodID methodId = app->GetJava()->Env->GetMethodID( MainActivityClass,
		"pauseMovie", "()V" );
	if ( !methodId )
	{
		LOG( "Couldn't find pauseMovie methodID" );
		return;
	}

	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, methodId );
#endif
	VideoPaused = true;
}

void Oculus360Videos::StopVideo()
{
	LOG( "StopVideo()" );

#if defined( OVR_OS_ANDROID )
	jmethodID methodId = app->GetJava()->Env->GetMethodID( MainActivityClass,
		"stopMovie", "()V" );
	if ( !methodId )
	{
		LOG( "Couldn't find stopMovie methodID" );
		return;
	}

	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, methodId );
#endif

	delete MovieTexture;
	MovieTexture = NULL;

	VideoPaused = false;
}

void Oculus360Videos::ResumeVideo()
{
	LOG( "ResumeVideo()" );

	GuiSys->CloseMenu( Browser, false );

#if defined( OVR_OS_ANDROID )
	jmethodID methodId = app->GetJava()->Env->GetMethodID( MainActivityClass,
		"resumeMovie", "()V" );
	if ( !methodId )
	{
		LOG( "Couldn't find resumeMovie methodID" );
		return;
	}

	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, methodId );
#endif
	VideoPaused = false;
}

void Oculus360Videos::StartVideo()
{
	if ( ActiveVideo )
	{
		SetMenuState( MENU_VIDEO_LOADING );
		VideoName = ActiveVideo->Url;
		LOG( "StartVideo( %s )", ActiveVideo->Url.ToCStr() );
		SoundEffectContext->Play( "sv_select" );

#if defined( OVR_OS_ANDROID )
		jmethodID startMovieMethodId = app->GetJava()->Env->GetMethodID( MainActivityClass,
			"startMovieFromNative", "(Ljava/lang/String;)V" );

		if ( !startMovieMethodId )
		{
			LOG( "Couldn't find startMovie methodID" );
			return;
		}

		LOG( "moviePath = '%s'", ActiveVideo->Url.ToCStr() );
		jstring jstr = app->GetJava()->Env->NewStringUTF( ActiveVideo->Url.ToCStr() );
		app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, startMovieMethodId, jstr );
		app->GetJava()->Env->DeleteLocalRef( jstr );
#endif
		VideoPaused = false;

		LOG( "StartVideo done" );
	}
}

void Oculus360Videos::SeekTo( const int seekPos )
{
	if ( ActiveVideo )
	{
#if defined( OVR_OS_ANDROID )
		jmethodID seekToMethodId = app->GetJava()->Env->GetMethodID( MainActivityClass,
			"seekToFromNative", "(I)V" );

		if ( !seekToMethodId )
		{
			LOG( "Couldn't find seekToMethodId methodID" );
			return;
		}

		app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, seekToMethodId, seekPos );
#endif
		LOG( "SeekTo %i done", seekPos );
	}
}

void Oculus360Videos::SetMenuState( const OvrMenuState state )
{
	OvrMenuState lastState = MenuState;
	MenuState = state;
	LOG( "%s to %s", MenuStateString( lastState ), MenuStateString( MenuState ) );
	switch ( MenuState )
	{
	case MENU_NONE:
		break;
	case MENU_BROWSER:
		Fader.ForceFinish();
		Fader.Reset();
		GuiSys->CloseMenu( VideoMenu, false );
		GuiSys->OpenMenu( OvrFolderBrowser::MENU_NAME );
		if ( ActiveVideo )
		{
			StopVideo();
			ActiveVideo = NULL;
		}
		GuiSys->GetGazeCursor().ShowCursor();
		break;
	case MENU_VIDEO_LOADING:
		if ( MovieTexture != NULL )
		{
			delete MovieTexture;
			MovieTexture = NULL;
		}
		GuiSys->CloseMenu( Browser, false );
		GuiSys->CloseMenu( VideoMenu, false );
		Fader.StartFadeOut();
		GuiSys->GetGazeCursor().HideCursor();
		break;
	case MENU_VIDEO_READY:
		break;
	case MENU_VIDEO_PLAYING:
		Fader.Reset();
		VideoMenuTimeLeft = VideoMenuVisibleTime;
		if ( !HmdMounted && !VideoPaused )
		{
			// Need to pause video, because HMD was unmounted during loading and we weren't able to
			// pause during loading.
			PauseVideo( false );
			GuiSys->GetGazeCursor().ShowCursor();
		}
		break;
	case MENU_VIDEO_PAUSE:
		GuiSys->OpenMenu( OvrVideoMenu::MENU_NAME );
		VideoMenu->RepositionMenu( app->GetLastViewMatrix() );
		PauseVideo( false );
		GuiSys->GetGazeCursor().ShowCursor();
		break;
	case MENU_VIDEO_RESUME:
		GuiSys->CloseMenu( VideoMenu, false );
		ResumeVideo();
		MenuState = MENU_VIDEO_PLAYING;
		GuiSys->GetGazeCursor().HideCursor();
		break;
	default:
		LOG( "Oculus360Videos::SetMenuState unknown state: %d", static_cast< int >( state ) );
		OVR_ASSERT( false );
		break;
	}
}

const char * menuStateNames [ ] =
{
	"MENU_NONE",
	"MENU_BROWSER",
	"MENU_VIDEO_LOADING",
	"MENU_VIDEO_READY",
	"MENU_VIDEO_PLAYING",
	"MENU_VIDEO_PAUSE",
	"MENU_VIDEO_RESUME",
	"NUM_MENU_STATES"
};

const char * Oculus360Videos::MenuStateString( const OvrMenuState state )
{
	OVR_ASSERT( state >= 0 && state < NUM_MENU_STATES );
	return menuStateNames[ state ];
}

void Oculus360Videos::OnVideoActivated( const OvrMetaDatum * videoData )
{
	ActiveVideo = videoData;
	StartVideo();
}

ovrFrameResult Oculus360Videos::Frame( const ovrFrameInput & vrFrame )
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

	// Process incoming messages until the queue is empty.
	for ( ; ; )
	{
		const char * msg = MessageQueue.GetNextMessage();
		if ( msg == NULL )
		{
			break;
		}
		Command( msg );
		free( (void *)msg );
	}

	FrameInput = vrFrame;

	// Disallow player foot movement, but we still want the head model
	// movement for the swipe view.
	ovrFrameInput vrFrameWithoutMove = FrameInput;
	vrFrameWithoutMove.Input.sticks[ 0 ][ 0 ] = 0.0f;
	vrFrameWithoutMove.Input.sticks[ 0 ][ 1 ] = 0.0f;

	// Check for new video frames
	// latch the latest movie frame to the texture.
	if ( MovieTexture != NULL && CurrentVideoWidth != 0 )
	{
		MovieTexture->Update();
		FrameAvailable = false;
	}

	// Update the Menu State
	if ( MenuState == MENU_VIDEO_PLAYING || MenuState == MENU_VIDEO_PAUSE )
	{
		if ( ( FrameInput.Input.buttonReleased & BUTTON_A ) ||
			( FrameInput.Input.buttonState & BUTTON_TOUCH_SINGLE ) )
		{
			SoundEffectContext->Play( "sv_release_active" );
			if ( IsVideoPlaying() )
			{
				SetMenuState( MENU_VIDEO_PAUSE );
			}
			else
			{
				SetMenuState( MENU_VIDEO_RESUME );
			}
		}
	}

	const bool mountingOn = !HmdMounted && ( FrameInput.DeviceStatus.DeviceIsDocked && FrameInput.DeviceStatus.HeadsetIsMounted );
	if ( mountingOn )
	{
		HmdMounted = true;
	}
	else
	{
		const bool mountingOff = HmdMounted && !FrameInput.DeviceStatus.HeadsetIsMounted;
		if ( mountingOff )
		{
			HmdMounted = false;
			if ( IsVideoPlaying() )
			{
				SetMenuState( MENU_VIDEO_PAUSE );
			}
		}
	}

	// State transitions
	if ( Fader.GetFadeState() != Fader::FADE_NONE )
	{
		Fader.Update( CurrentFadeRate, FrameInput.DeltaSeconds );
	}
	else if ( ( MenuState == MENU_VIDEO_READY ) && ( Fader.GetFadeAlpha() == 0.0f ) && ( MovieTexture != NULL ) )
	{
		SetMenuState( MENU_VIDEO_PLAYING );
		app->RecenterYaw( true );
	}
	CurrentFadeLevel = Fader.GetFinalAlpha();

	// Override the material on the background scene to allow the model to fade during state transitions.
	{
		const ModelFile * modelFile = Scene.GetWorldModel()->Definition;
		for ( int i = 0; i < modelFile->Models.GetSizeI(); i++ )
		{
			for ( int j = 0; j < modelFile->Models[i].surfaces.GetSizeI(); j++ )
			{
				// TODO: Is this coding we really want to encourage?  
				ovrGraphicsCommand & graphicsCommand = *const_cast< ovrGraphicsCommand * >(&modelFile->Models[i].surfaces[j].surfaceDef.graphicsCommand);
				graphicsCommand.Program = SingleColorTextureProgram;
				graphicsCommand.uniformSlots[0] = SingleColorTextureProgram.uColor;

				for ( int k = 0; k < 3; k++ )
				{
					graphicsCommand.uniformValues[0][k] = CurrentFadeLevel;
				}
				graphicsCommand.uniformValues[0][3] = 1.0f;
			}
		}
	}

	Scene.Frame( vrFrameWithoutMove );

	ovrFrameResult res;
	Scene.GetFrameMatrices( vrFrameWithoutMove.FovX, vrFrameWithoutMove.FovY, res.FrameMatrices );
	Scene.GenerateFrameSurfaceList( res.FrameMatrices, res.Surfaces );

	// Update gui systems after the app frame, but before rendering anything.
	GuiSys->Frame( FrameInput, res.FrameMatrices.CenterView );

	const bool videoShowing = ( MenuState == MENU_VIDEO_PLAYING || MenuState == MENU_VIDEO_PAUSE ) && ( MovieTexture != NULL );

	//-------------------------------
	// Rendering
	//-------------------------------

	res.FrameIndex = FrameInput.FrameNumber;
	res.DisplayTime = vrFrame.PredictedDisplayTimeInSeconds;
	res.SwapInterval = app->GetSwapInterval();

	res.FrameFlags = 0;
	res.LayerCount = 0;

	ovrLayerProjection2 & worldLayer = res.Layers[ res.LayerCount++ ].Projection;
	worldLayer = vrapi_DefaultLayerProjection2();

	worldLayer.HeadPose = FrameInput.Tracking.HeadPose;
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		worldLayer.Textures[eye].ColorSwapChain = FrameInput.ColorTextureSwapChain[eye];
		worldLayer.Textures[eye].SwapChainIndex = FrameInput.TextureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = FrameInput.TexCoordsFromTanAngles;
	}
	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	if ( videoShowing )
	{
		GlobeProgramTexture[0] = GlTexture( MovieTexture->GetTextureId(), GL_TEXTURE_EXTERNAL_OES, 0, 0 );
		GlobeProgramTexture[1] = GlTexture( BackgroundTexId, GL_TEXTURE_2D, 0, 0 );
		GlobeProgramColor = Vector4f( 1.0f );
		GlobeProgramMatrices[0] = TexmForVideo( 0 );
		GlobeProgramMatrices[1] = TexmForVideo( VideoMenu->IsOpenOrOpening() ? 0 : 1 );

		if ( BackgroundWidth == BackgroundHeight )
		{
			GlobeSurfaceDef.graphicsCommand.Program = FadedPanoramaProgram;
			GlobeSurfaceDef.graphicsCommand.UniformData[0].Data = &GlobeProgramMatrices[0];
			GlobeSurfaceDef.graphicsCommand.UniformData[0].Count = 2;
			GlobeSurfaceDef.graphicsCommand.UniformData[1].Data = &GlobeProgramColor;
			GlobeSurfaceDef.graphicsCommand.UniformData[2].Data = &GlobeProgramTexture[0];
			GlobeSurfaceDef.graphicsCommand.UniformData[3].Data = &GlobeProgramTexture[1];
		}
		else
		{
			GlobeSurfaceDef.graphicsCommand.Program = PanoramaProgram;
			GlobeSurfaceDef.graphicsCommand.UniformData[0].Data = &GlobeProgramMatrices[0];
			GlobeSurfaceDef.graphicsCommand.UniformData[0].Count = 2;
			GlobeSurfaceDef.graphicsCommand.UniformData[1].Data = &GlobeProgramColor;
			GlobeSurfaceDef.graphicsCommand.UniformData[2].Data = &GlobeProgramTexture[0];
		}

		res.Surfaces.PushBack( ovrDrawSurface( &GlobeSurfaceDef ) );
	}

	GuiSys->AppendSurfaceList( res.FrameMatrices.CenterView, &res.Surfaces );

	return res;
}

} // namespace OVR
