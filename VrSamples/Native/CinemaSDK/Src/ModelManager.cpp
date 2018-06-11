/************************************************************************************

Filename    :   ModelManager.cpp
Content     :
Created     :	7/3/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "Kernel/OVR_String_Utils.h"
#include "ModelManager.h"
#include "CinemaApp.h"
#include "PackageFiles.h"

#if defined( OVR_OS_ANDROID )
#include <dirent.h>
#endif

namespace OculusCinema {

static const char * TheatersDirectory = "Oculus/Cinema/Theaters";

//=======================================================================================

ModelManager::ModelManager( CinemaApp &cinema ) :
	Cinema( cinema ),
	Theaters(),
	BoxOffice( NULL ),
	VoidScene( NULL ),
	LaunchIntent(),
	DefaultSceneModel( NULL )

{
}

ModelManager::~ModelManager()
{
}

void ModelManager::OneTimeInit( const char * launchIntent )
{
	LOG( "ModelManager::OneTimeInit" );
	const double start = SystemClock::GetTimeInSeconds();
	LaunchIntent = launchIntent;

	DefaultSceneModel = new ModelFile( "default" );

	LoadModels();

	LOG( "ModelManager::OneTimeInit: %i theaters loaded, %3.1f seconds", Theaters.GetSizeI(),  SystemClock::GetTimeInSeconds() - start );
}

void ModelManager::OneTimeShutdown()
{
	LOG( "ModelManager::OneTimeShutdown" );

	// Free GL resources

	for( UPInt i = 0; i < Theaters.GetSize(); i++ )
	{
		delete Theaters[ i ];
	}
}

void ModelManager::LoadModels()
{
	LOG( "ModelManager::LoadModels" );
	const double start =  SystemClock::GetTimeInSeconds();

	BoxOffice = LoadScene( "assets/scenes/BoxOffice.ovrscene", false, true, true );
	BoxOffice->UseSeats = false;
	BoxOffice->LobbyScreen = true;

	if ( LaunchIntent.GetLength() > 0 )
	{
		Theaters.PushBack( LoadScene( LaunchIntent.ToCStr(), true, true, false ) );
	}
	else
	{
		// we want our theaters to show up first
		Theaters.PushBack( LoadScene( "assets/scenes/home_theater.ovrscene", true, false, true ) );

		// create void scene
		VoidScene = new SceneDef();
		VoidScene->SceneModel = new ModelFile( "Void" );
		VoidScene->UseSeats = false;
		VoidScene->UseDynamicProgram = false;
		VoidScene->UseScreenGeometry = false;
		VoidScene->UseFreeScreen = true;
		//        VoidScene->UseVRScreen = true;


		int width = 0, height = 0;
		VoidScene->IconTexture = LoadTextureFromApplicationPackage( "assets/VoidTheater.png",
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), width, height );

		BuildTextureMipmaps( GlTexture( VoidScene->IconTexture, width, height ) );
		MakeTextureTrilinear( GlTexture( VoidScene->IconTexture, width, height ) );
		MakeTextureClamped( GlTexture( VoidScene->IconTexture, width, height ) );

		Theaters.PushBack( VoidScene );

		// create void scene
		VRScene = new SceneDef();
		VRScene->SceneModel = new ModelFile( "VR" );
		VRScene->UseSeats = false;
		VRScene->UseDynamicProgram = false;
		VRScene->UseScreenGeometry = false;
		VRScene->UseFreeScreen = true;
		VRScene->UseVRScreen = true;

		VRScene->IconTexture = LoadTextureFromApplicationPackage( "assets/VRTheater.png",
																  TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), width, height );

		BuildTextureMipmaps( VRScene->IconTexture );
		MakeTextureTrilinear( VRScene->IconTexture );
		MakeTextureClamped( VRScene->IconTexture );

		Theaters.PushBack( VRScene );


		// load all scenes on startup, so there isn't a delay when switching theaters
		ScanDirectoryForScenes( Cinema.ExternalRetailDir( TheatersDirectory ), true, false, Theaters );
		ScanDirectoryForScenes( Cinema.RetailDir( TheatersDirectory ), true, false, Theaters );
		ScanDirectoryForScenes( Cinema.SDCardDir( TheatersDirectory ), true, false, Theaters );
	}

	LOG( "ModelManager::LoadModels: %i theaters loaded, %3.1f seconds", Theaters.GetSizeI(),  SystemClock::GetTimeInSeconds() - start );
}

void ModelManager::ScanDirectoryForScenes( const char * directory, bool useDynamicProgram, bool useScreenGeometry, Array<SceneDef *> &scenes ) const
{
#if defined( OVR_OS_ANDROID )
	DIR * dir = opendir( directory );
	if ( dir != NULL )
	{
		struct dirent * entry;
		while( ( entry = readdir( dir ) ) != NULL ) {
			String filename = entry->d_name;
			String ext = filename.GetExtension().ToLower();
			if ( ( ext == ".ovrscene" ) )
			{
				String fullpath = directory;
				fullpath.AppendString( "/" );
				fullpath.AppendString( filename.ToCStr() );
				SceneDef *def = LoadScene( fullpath.ToCStr(), useDynamicProgram, useScreenGeometry, false );
				scenes.PushBack( def );
			}
		}

		closedir( dir );
	}
#else
    WIN32_FIND_DATA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    String scanDirectory = String( directory );
    scanDirectory.AppendChar( '*' );

    hFind = FindFirstFile( scanDirectory.ToCStr(), &ffd );
    if ( INVALID_HANDLE_VALUE != hFind )
    {
        do
        {
            if ( !( ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
            {
                String filename = ffd.cFileName;

                String ext = filename.GetExtension().ToLower();
                if ( ( ext == ".ovrscene" ) )
                {
                    String fullpath = directory;
                    fullpath.AppendString( "/" );
                    fullpath.AppendString( filename.ToCStr() );
					SceneDef *def = LoadScene( fullpath.ToCStr(), useDynamicProgram, useScreenGeometry, false );
					scenes.PushBack( def );
                }
            }
        } while ( FindNextFile( hFind, &ffd ) != 0 );
    }
#endif
}

SceneDef * ModelManager::LoadScene( const char *sceneFilename, bool useDynamicProgram, bool useScreenGeometry, bool loadFromApplicationPackage ) const
{
	String filename;

	if ( loadFromApplicationPackage && !ovr_PackageFileExists( sceneFilename ) )
	{
		LOG( "Scene %s not found in application package.  Checking sdcard.", sceneFilename );
		loadFromApplicationPackage = false;
	}

	if ( loadFromApplicationPackage )
	{
		filename = sceneFilename;
	}
	else if ( ( sceneFilename != NULL ) && ( *sceneFilename == '/' ) ) 	// intent will have full path for scene file, so check for /
	{
		filename = sceneFilename;
	}
	else if ( Cinema.FileExists( Cinema.ExternalRetailDir( sceneFilename ) ) )
	{
		filename = Cinema.ExternalRetailDir( sceneFilename );
	}
	else if ( Cinema.FileExists( Cinema.RetailDir( sceneFilename ) ) )
	{
		filename = Cinema.RetailDir( sceneFilename );
	}
	else
	{
		filename = Cinema.SDCardDir( sceneFilename );
	}

	LOG( "Adding scene: %s, %s", filename.ToCStr(), sceneFilename );

	SceneDef *def = new SceneDef();
	def->Filename = sceneFilename;
	def->UseSeats = true;
	def->UseDynamicProgram = useDynamicProgram;

	MaterialParms materialParms;
	materialParms.UseSrgbTextureFormats = Cinema.app->GetFramebufferIsSrgb();
	// Improve the texture quality with anisotropic filtering.
	materialParms.EnableDiffuseAniso = true;
	// The emissive texture is used as a separate lighting texture and should not be LOD clamped.
	materialParms.EnableEmissiveLodClamp = false;

	ModelGlPrograms glPrograms = ( useDynamicProgram ) ? Cinema.ShaderMgr.DynamicPrograms : Cinema.ShaderMgr.DefaultPrograms;

	String iconFilename = StringUtils::SetFileExtensionString( filename.ToCStr(), "png" );

	int textureWidth = 0, textureHeight = 0;

	if ( loadFromApplicationPackage )
	{
		def->SceneModel = LoadModelFileFromApplicationPackage( filename.ToCStr(), glPrograms, materialParms );
		def->IconTexture = LoadTextureFromApplicationPackage( iconFilename.ToCStr(), TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), textureWidth, textureHeight );
	}
	else
	{
		def->SceneModel = LoadModelFile( filename.ToCStr(), glPrograms, materialParms );
		def->IconTexture = LoadTextureFromBuffer( iconFilename.ToCStr(), MemBufferFile( iconFilename.ToCStr() ),
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), textureWidth, textureHeight );
	}

	if ( def->SceneModel == nullptr )
	{
		WARN( "Could not load scenemodel %s", filename.ToCStr() );
		def->SceneModel = new ModelFile( "Default Scene Model" );
	}

	if ( def->IconTexture != 0 )
	{
		LOG( "Loaded external icon for theater: %s", iconFilename.ToCStr() );
	}
	else
	{
		const ModelTexture * iconTexture = def->SceneModel->FindNamedTexture( "icon" );
		if ( iconTexture != NULL )
		{
			def->IconTexture = iconTexture->texid;
		}
		else
		{
			LOG( "No icon in scene.  Loading default." );

			int	width = 0, height = 0;
			def->IconTexture = LoadTextureFromApplicationPackage( "assets/noimage.png",
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), width, height );
		}
	}

	BuildTextureMipmaps( def->IconTexture );
	MakeTextureTrilinear( def->IconTexture );
	MakeTextureClamped( def->IconTexture );

	def->UseScreenGeometry = useScreenGeometry;
	def->UseFreeScreen = false;

	return def;
}

const SceneDef & ModelManager::GetTheater( int index ) const
{
	if ( index < Theaters.GetSizeI() )
	{
		return *Theaters[ index ];
	}

	// default to the Void Scene
	return *VoidScene;
}

} // namespace OculusCinema
