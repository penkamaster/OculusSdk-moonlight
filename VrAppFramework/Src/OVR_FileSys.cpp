/************************************************************************************

Filename    :   OVR_FileSys.cpp
Content     :   Abraction layer for file systems.
Created     :   July 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_FileSys.h"

#include "OVR_Stream_Impl.h"
#include "Kernel/OVR_UTF8Util.h"
#include "Kernel/OVR_Array.h"
#include <cctype>	// for isdigit, isalpha
#include "OVR_Uri.h"
#include "PathUtils.h"
#include "Kernel/OVR_LogUtils.h"

#if defined( OVR_OS_ANDROID )
#	include "Android/JniUtils.h"
#elif defined( OVR_OS_WIN32 )
#	include <direct.h>
#	include <io.h>
#endif

namespace OVR {


//==============================================================
// ovrFileSysLocal
class ovrFileSysLocal : public ovrFileSys
{
public:
	// this is yucky right now because it's Java-specific, even though windows doesn't care about it.
							ovrFileSysLocal( ovrJava const & javaContext );	
	virtual					~ovrFileSysLocal();

	virtual ovrStream *		OpenStream( char const * uri, ovrStreamMode const mode );
	virtual void			CloseStream( ovrStream * & stream );
	virtual bool			ReadFile( char const * uri, MemBufferT< uint8_t > & outBuffer );
	virtual bool			FileExists( char const * uri );
	virtual bool			GetLocalPathForURI( char const * uri, String &outputPath );

	virtual void			Shutdown();

private:
	Array< ovrUriScheme* >	Schemes;
	JavaVM *				Jvm{ nullptr };
	jobject					ActivityObject{ 0 };

private:
	int						FindSchemeIndexForName( char const * schemeName ) const;
	ovrUriScheme *			FindSchemeForName( char const * name ) const;
};

#if defined( OVR_OS_WIN32 )
static void AddRelativePathToHost( ovrUriScheme_File * scheme, char const * hostName, char const * baseDir, char const * relativePath )
{
	char rebasedPath[MAX_PATH];
	OVR_sprintf( rebasedPath, sizeof( rebasedPath ), "%s/%s", baseDir, relativePath );

	// collapse the path -- fopen() was not working with relative paths, though it should?
	char rebasedPathCanonical[MAX_PATH];
	ovrPathUtils::CollapsePath( rebasedPath, rebasedPathCanonical, sizeof( rebasedPathCanonical ) );

	char uri[ovrFileSysLocal::OVR_MAX_PATH_LEN];
	OVR_sprintf( uri, sizeof( uri ), "file:///%s", rebasedPathCanonical );	

	if ( !scheme->HostExists( hostName ) )
	{
		scheme->OpenHost( hostName, uri );
	}
	else
	{
		scheme->AddHostSourceUri( hostName, uri);	
	}
}
#endif

// TODO_SYS_UTILS: Expose this via a sys_prop instead of hardcoding?
#define PUI_PACKAGE_NAME			"com.oculus.systemactivities"

//==============================
// ovrFileSysLocal::ovrFileSysLocal
ovrFileSysLocal::ovrFileSysLocal( ovrJava const & javaContext ) : Jvm( javaContext.Vm )
{
	// always do unit tests on startup to assure nothing has been broken
	ovrUri::DoUnitTest();

#if defined( OVR_OS_ANDROID )
	ActivityObject = javaContext.Env->NewGlobalRef( javaContext.ActivityObject );

	// add the apk scheme 
	ovrUriScheme_Apk * scheme = new ovrUriScheme_Apk( "apk" );

	// add a host for the executing application's scheme
	char curPackageName[OVR_MAX_PATH_LEN];
	ovr_GetCurrentPackageName( javaContext.Env, javaContext.ActivityObject, curPackageName, sizeof( curPackageName ) );
	char curPackageCodePath[OVR_MAX_PATH_LEN];
	ovr_GetPackageCodePath( javaContext.Env, javaContext.ActivityObject, curPackageCodePath, sizeof( curPackageCodePath ) );

	// not sure if this is necessary... shouldn't the application always have permission to open its own scheme?
/*
	String outPath;
	const bool validCacheDir = StoragePaths->GetPathIfValidPermission(
			EST_INTERNAL_STORAGE, EFT_CACHE, "", permissionFlags_t( PERMISSION_WRITE ) | PERMISSION_READ, outPath );
	ovr_OpenApplicationPackage( temp, validCacheDir ? outPath.ToCStr() : NULL );
*/
	char curPackageUri[OVR_MAX_URI_LEN];
	OVR_sprintf( curPackageUri, sizeof( curPackageUri ), "file://%s", curPackageCodePath );
	if ( !scheme->OpenHost( "localhost", curPackageUri ) )
	{
		LOG( "Failed to OpenHost for host '%s', uri '%s'", "localhost", curPackageUri );
		OVR_ASSERT( false );
	}

	// add the hosts for the system activities apk

	/// FIXME: why do we try to add the PUI_PACKAGE_NAME package and then curPackageName? Shouldn't
	/// the PUI_PACKAGE_NAME (i.e. System Activities) always exist and shouldn't we always add
	/// the current package as a valid host name anyway?

	// NOTE: we do not add a "com.<company>.<appname>" ( i.e. the Android app's package name ) host 
	// for Windows because we don't really have a good way of knowing what the Android package name
	// on Windows without parsing the AndroidManifest.xml explicitly for that reason. This means
	// using apk://com.<company>.<appname> for the scheme and host will fail on non-Android builds.
	// This, in turn, means that any access to the apps own assets should just be done with 
	// apk://localhost/ in order to be cross-platform compatible. Because of that, we do not add
	// "com.<company>.<appname>" as a host on Android, either (since it would just fail on Windows).

	{
		for ( int i = 0; i < 2; ++i )
		{
			char const * packageName = ( i == 0 ) ? PUI_PACKAGE_NAME : curPackageName;
			char packagePath[OVR_MAX_PATH_LEN];
			packagePath[0] = '\0';
			if ( ovr_GetInstalledPackagePath( javaContext.Env, javaContext.ActivityObject, packageName, packagePath, sizeof( packagePath ) ) )
			{
				char packageUri[sizeof( packagePath ) + 7 ];
				OVR_sprintf( packageUri, sizeof( packageUri ), "file://%s", packagePath );

				scheme->OpenHost( packageName, packageUri );
				break;
			}
		}
	}

	Schemes.PushBack( scheme );

	// add the host for font assets by opening a stream and trying to load res/raw/font_location.txt from the System Activites apk.
	// If this file exists then
	{
		MemBufferT< uint8_t > buffer;
		char fileName[256];
		OVR::OVR_sprintf( fileName, sizeof( fileName ), "apk://%s/res/raw/font_location.txt", PUI_PACKAGE_NAME );
		char fontPackageName[1024];		
		bool success = ReadFile( fileName, buffer );
		if ( success && buffer.GetSize() > 0 )
		{
			OVR::OVR_strncpy( fontPackageName, sizeof( fontPackageName ), ( char const * )( static_cast< uint8_t const * >( buffer ) ), buffer.GetSize() );
			LOG( "Found font package name '%s'", fontPackageName );
		} else {
			// default to the SystemActivities apk.
			OVR::OVR_strcpy( fontPackageName, sizeof( fontPackageName ), PUI_PACKAGE_NAME );
		}

		char packagePath[OVR_MAX_PATH_LEN];
		packagePath[0] = '\0';
		if ( ovr_GetInstalledPackagePath( javaContext.Env, javaContext.ActivityObject, fontPackageName, packagePath, sizeof( packagePath ) ) )
		{
			// add this package to our scheme as a host so that fonts can be loaded from it
			char packageUri[sizeof( packagePath ) + 7 ];
			OVR_sprintf( packageUri, sizeof( packageUri ), "file://%s", packagePath );				

			// add the package name as an explict host if it doesn't already exists -- it will already exist if the package name
			// is not overrloaded by font_location.txt (i.e. the fontPackageName will have defaulted to PUI_PACKAGE_NAME )
			if ( !scheme->HostExists( fontPackageName ) ) {
				scheme->OpenHost( fontPackageName, packageUri );
			}
			scheme->OpenHost( "font", packageUri );

			LOG( "Added host '%s' for fonts @'%s'", fontPackageName, packageUri );
		}
	}
#elif defined( OVR_OS_WIN32 )
	ovrPathUtils::DoUnitTests();

	// Assume the working dir has an assets/ and res/ folder in it as is common on Android. Normally
	// the working folder would be Projects/Android.
	char curWorkingDir[MAX_PATH];
	_getcwd( curWorkingDir, sizeof( curWorkingDir ) );
	char uriWorkingDir[MAX_PATH];
	ovrPathUtils::FixSlashesForUri( curWorkingDir, uriWorkingDir, sizeof( uriWorkingDir ) );

	// if the asset package exists in the current working dir, add it as an apk scheme instead of using
	// raw files
	const char * const PACKAGE_EXTENSION = "pak";
	char packagePath[MAX_PATH];
	OVR_sprintf( packagePath, sizeof( packagePath ), "%s\\%s.%s", curWorkingDir, "assets", PACKAGE_EXTENSION );
	if ( _access( packagePath, 04 ) == 0 )
	{
		// package exists
		ovrUriScheme_Apk * scheme = new ovrUriScheme_Apk( "apk" );
		
		char packageUri[OVR_MAX_URI_LEN];
		OVR_sprintf( packageUri, sizeof( packageUri ), "file://%s", packagePath );
		ovrPathUtils::FixSlashesForUriInPlace( packageUri );
		if ( !scheme->OpenHost( "localhost", packageUri ) )
		{
			LOG( "Failed to OpenHost for host '%s', uri '%s'", "localhost", packageUri );
			OVR_ASSERT( false );
		}

		// NOTE: we do not add a "com.<company>.<appname>" ( i.e. the Android app's package name ) host 
		// for Windows because we don't really have a good way of knowing what the Android package name
		// on Windows without parsing the AndroidManifest.xml explicitly for that reason. This means
		// using apk://com.<company>.<appname> for the scheme and host will fail on non-Android builds.
		// This, in turn, means that any access to the apps own assets should just be done with 
		// apk://localhost/ in order to be cross-platform compatible. Because of that, we do not add
		// "com.<company>.<appname>" as a host on Android, either (since it would just fail on Windows).
		
		// Currently "com.oculus.systemactivities" and "font" hosts assume the SA assets are simply 
		// packed into the application's asset package.
		if ( !scheme->OpenHost( PUI_PACKAGE_NAME, packageUri ) )
		{
			LOG( "Failed to OpenHost for host '%s', uri '%s'", PUI_PACKAGE_NAME, packageUri );
			OVR_ASSERT( false );
		}
		if ( !scheme->OpenHost( "font", packageUri ) )
		{
			LOG( "Failed to OpenHost for host '%s', uri '%s'", "font", packageUri );
			OVR_ASSERT( false );
		}

        Schemes.PushBack(scheme);
    }
    else
    {
		// add the apk scheme for the working path
		ovrUriScheme_File * scheme = new ovrUriScheme_File("apk");

		// On Android we have several different APK hosts:
		// - apk://com.oculus.systemactivities/ : this may hold vrapi.so, font_location.txt, or font data.
		// - apk://font/ : this may hold font data, if font data has been moved out of SystemActivities apk, 
		//   so all fonts should be loaded via this host.
		// - apk://localhost/ : the default host that maps to the application's own apk.
		char dataUri[OVR_MAX_PATH_LEN];
		OVR_sprintf( dataUri, sizeof( dataUri ), "file:///%s", uriWorkingDir );
          
		// HACK: this is currently relying on the relative path to VrAppFramework being the same for all projects, which it's not.
		// FIXME: change this to use command line parameters that specify additional paths.
		AddRelativePathToHost( scheme, "localhost", curWorkingDir, "../../../VrAppFramework" );
		AddRelativePathToHost( scheme, "localhost", curWorkingDir, "../../../VrAppSupport/VrGUI" );
		scheme->AddHostSourceUri( "localhost", dataUri );

		AddRelativePathToHost( scheme, PUI_PACKAGE_NAME, curWorkingDir, "../../../VrAppFramework" );
		AddRelativePathToHost( scheme, PUI_PACKAGE_NAME, curWorkingDir, "../../../VrAppSupport/VrGUI" );
		// MA: This path works if target is within Projects
		AddRelativePathToHost( scheme, PUI_PACKAGE_NAME, curWorkingDir, "../../../../../../VrAppFramework" );
		AddRelativePathToHost( scheme, PUI_PACKAGE_NAME, curWorkingDir, "../../../../../../VrAppSupport/VrGUI" );
		scheme->AddHostSourceUri( PUI_PACKAGE_NAME, dataUri );

		AddRelativePathToHost( scheme, "font", curWorkingDir, "../../../VrAppFramework" );
		AddRelativePathToHost( scheme, "font", curWorkingDir, "../../../VrAppSupport/VrGUI" );
		// MA: This path works if target is within Projects
		AddRelativePathToHost( scheme, "font", curWorkingDir, "../../../../../../VrAppFramework");
		AddRelativePathToHost( scheme, "font", curWorkingDir, "../../../../../../VrAppSupport/VrGUI");
		scheme->AddHostSourceUri( "font", dataUri );

		Schemes.PushBack( scheme );	
	}
#else
#error Unsupported platform!
#endif
}

//==============================
// ovrFileSysLocal::ovrFileSysLocal
ovrFileSysLocal::~ovrFileSysLocal()
{
#if defined( OVR_OS_ANDROID )
	TempJniEnv env{ Jvm };
	env->DeleteGlobalRef( ActivityObject );
#endif
}

//==============================
// ovrFileSysLocal::OpenStream
ovrStream *	ovrFileSysLocal::OpenStream( char const * uri, ovrStreamMode const mode )
{
	// parse the Uri to find the scheme
	char scheme[OVR_MAX_SCHEME_LEN];
	char host[OVR_MAX_HOST_NAME_LEN];
	char path[OVR_MAX_PATH_LEN];
	int port = 0;
	ovrUri::ParseUri( uri, scheme, sizeof( scheme ), NULL, 0, NULL, 0, host, sizeof( host ), 
			port, path, sizeof( path ), NULL, 0, NULL, 0 );

	ovrUriScheme * uriScheme = FindSchemeForName( scheme );
	if ( uriScheme == NULL )
	{
		LOG( "Uri '%s' missing scheme! Assuming apk scheme!", uri );
		uriScheme = FindSchemeForName( "apk" );
		if ( uriScheme == NULL )
		{
			return NULL;
		}
	}

#if defined( OVR_OS_ANDROID )
	// If apk scheme, need to check if this is a package we haven't seen before, and add a host if so.
	if ( OVR_stricmp( scheme, "apk" ) == 0 )
	{
		if ( !uriScheme->HostExists( host ) )
		{
			TempJniEnv env{ Jvm };
			char packagePath[OVR_MAX_PATH_LEN];
			packagePath[0] = '\0';
			if ( ovr_GetInstalledPackagePath( env, ActivityObject, host, packagePath, sizeof( packagePath ) ) )
			{
				char packageUri[sizeof( packagePath ) + 7 ];
				OVR_sprintf( packageUri, sizeof( packageUri ), "file://%s", packagePath );
				uriScheme->OpenHost( host, packageUri );
			}
		}
	}
#endif

	ovrStream * stream = uriScheme->AllocStream();
	if ( stream == NULL )
	{
		OVR_ASSERT( stream != NULL );
		return NULL;
	}
	if ( !stream->Open( uri, mode ) )
	{
		delete stream;
		return NULL;
	}
	return stream;
}

//==============================
// ovrFileSysLocal::CloseStream
void ovrFileSysLocal::CloseStream( ovrStream * & stream )
{
	if ( stream != NULL )
	{
		stream->Close();
		delete stream;
		stream = NULL;
	}
}

//==============================
// ovrFileSysLocal::ReadFile
bool ovrFileSysLocal::ReadFile( char const * uri, MemBufferT< uint8_t > & outBuffer )
{
	ovrStream * stream = OpenStream( uri, OVR_STREAM_MODE_READ );
	if ( stream == NULL )
	{
		return false;
	}
	bool success = stream->ReadFile( uri, outBuffer );
	CloseStream( stream );
	return success;
}

//==============================
// ovrFileSysLocal::FileExists
bool ovrFileSysLocal::FileExists( char const * uri )
{
	ovrStream * stream = OpenStream( uri, OVR_STREAM_MODE_READ );
	if ( stream == NULL )
	{
		return false;
	}
	CloseStream( stream );
	return true;
}

//==============================
// ovrFileSysLocal::GetLocalPathForURI
bool ovrFileSysLocal::GetLocalPathForURI( char const * uri, String &outputPath )
{
	// parse the Uri to find the scheme
	char scheme[OVR_MAX_SCHEME_LEN];
	char host[OVR_MAX_HOST_NAME_LEN];
	char path[OVR_MAX_PATH_LEN];
	int port = 0;
	ovrUri::ParseUri( uri, scheme, sizeof( scheme ), NULL, 0, NULL, 0, host, sizeof( host ), 
			port, path, sizeof( path ), NULL, 0, NULL, 0 );

	ovrUriScheme * uriScheme = FindSchemeForName( scheme );
	if ( uriScheme == NULL )
	{
		LOG( "GetLocalPathForURI: Uri '%s' missing scheme!", uri );
		return false;
	}

	// FIXME: It would be better to not have to allocate a stream to just get the path
	ovrStream * stream = uriScheme->AllocStream();
	if ( stream == NULL )
	{
		OVR_ASSERT( stream != NULL );
		return false;
	}

	const bool result = stream->GetLocalPathFromUri( uri, outputPath );
	delete stream;

	return result;	
}

//==============================
// ovrFileSysLocal::FindSchemeIndexForName
int ovrFileSysLocal::FindSchemeIndexForName( char const * schemeName ) const
{
	for ( int i = 0; i < Schemes.GetSizeI(); ++i )
	{
		if ( OVR_stricmp( Schemes[i]->GetSchemeName(), schemeName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

//==============================
// ovrFileSysLocal::FindSchemeForName
ovrUriScheme * ovrFileSysLocal::FindSchemeForName( char const * name ) const
{
	int index = FindSchemeIndexForName( name );
	return index < 0 ? NULL : Schemes[index];
}

//==============================
// ovrFileSysLocal::Shutdown
void ovrFileSysLocal::Shutdown()
{
	for ( int i = 0; i < Schemes.GetSizeI(); ++i )
	{
		Schemes[i]->Shutdown();
		delete Schemes[i];
		Schemes[i] = NULL;
	}
	Schemes.Clear();
}

//==============================================================================================
// ovrFileSys
//==============================================================================================

//==============================
// ovrFileSys::Create
ovrFileSys * ovrFileSys::Create( ovrJava const & javaContext )
{
	ovrFileSys * fs = new ovrFileSysLocal( javaContext );
	return fs;
}

//==============================
// ovrFileSys::Destroy
void ovrFileSys::Destroy( ovrFileSys * & fs )
{
	if ( fs != NULL )
	{
		ovrFileSysLocal * fsl = static_cast< ovrFileSysLocal* >( fs );
		fsl->Shutdown();
		delete fs;
		fs = NULL;
	}
}

} // namespace OVR
