/************************************************************************************

Filename    :   PathUtils.cpp
Content     :
Created     :   November 26, 2014
Authors     :   Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "PathUtils.h"

#include <stdio.h>
#include "Android/JniUtils.h"
#include "VrCommon.h"
#include "App.h"

#include "VrApi.h"				// for vrapi_GetSystemPropertyString
#include "VrApi_SystemUtils.h"	// for vrapi_ShowFatalError

#if defined( OVR_OS_WIN32 )
#include <direct.h>
#include "Shlwapi.h"
#endif

namespace OVR {

static const char * StorageName[EST_COUNT] =
{
	"Phone Internal", 	// "/data/data/"
	"Phone External",  	// "/storage/emulated/0" or "/sdcard"
	"SD Card External" 	// "/storage/extSdCard", "", or a dynamic UUID if Android-M
};

static const char * FolderName[EFT_COUNT] =
{
	"Root",
	"Files",
	"Cache"
};

static const char * FolderStorageMethodName[EST_COUNT][EFT_COUNT] = 
{
	{ "getInternalStorageRootDir",			"getInternalStorageFilesDir",			"getInternalStorageCacheDir" },
	{ "getPrimaryExternalStorageRootDir",	"getPrimaryExternalStorageFilesDir",	"getPrimaryExternalStorageCacheDir" },
	{ "getSecondaryExternalStorageRootDir",	"getSecondaryExternalStorageFilesDir",	"getSecondaryExternalStorageCacheDir" },
};

static const char * FolderStorageSignature = "(Landroid/app/Activity;)Ljava/lang/String;";

static String GetDir( ovrStorageType storageType, ovrFolderType folderType, jclass VrActivityClass, const ovrJava * java )
{
	OVR_ASSERT( storageType < EST_COUNT );
	OVR_ASSERT( folderType < EFT_COUNT );

#if defined( OVR_OS_ANDROID )

	if ( storageType == EST_SECONDARY_EXTERNAL_STORAGE && folderType == EFT_ROOT )
	{
		// On Android-M, external sdcard path is dynamic and is queried through VRSVC.
		return vrapi_GetSystemPropertyString( java, VRAPI_SYS_PROP_EXT_SDCARD_PATH );
	}
	else
	{
		const char * methodName = FolderStorageMethodName[storageType][folderType];
		const char * methodSignature = FolderStorageSignature;

		jmethodID methodID = ovr_GetStaticMethodID( java->Env, VrActivityClass, methodName, methodSignature );
		if ( methodID != NULL )
		{
			jstring javaString = (jstring)java->Env->CallStaticObjectMethod( VrActivityClass, methodID, java->ActivityObject );
			if ( javaString != NULL )
			{
				JavaUTFChars returnString( java->Env, javaString );
				return returnString.ToStr();
			}
		}
		// it is a fatal error if Java doesn't return a valid string
		java->Env->ExceptionClear();
		// FIXME: Should the app be handling this explicitly?
		StringBuffer errorMessage;
		errorMessage.AppendFormat( "Failed to get storage for %s", methodName );
		vrapi_ShowFatalError( java, "failOutOfStorage", errorMessage.ToCStr(), __FILE__, __LINE__ );
	}

	return String();
#elif defined( OVR_OS_WIN32  )
	if ( folderType == EFT_CACHE )
	{
		TCHAR lpTempPathBuffer[ MAX_PATH ];
		TCHAR buffer[ MAX_PATH ] = { 0 };
		TCHAR * executableName;
		DWORD bufSize = sizeof( buffer ) / sizeof( *buffer );

		// Get the fully-qualified path of the executable
		GetModuleFileName( NULL, buffer, bufSize );

		// Go to the beginning of the file name
		executableName = PathFindFileName( buffer );
	
		// Set the dot before the extension to 0 (terminate the string there)
		*( PathFindExtension( executableName ) ) = 0;

		DWORD dwRetVal = GetTempPath( MAX_PATH, lpTempPathBuffer );
		if ( !( dwRetVal > MAX_PATH || ( dwRetVal == 0 ) ) )
		{
			char cacheDir[ MAX_PATH ];
			OVR_sprintf( cacheDir, sizeof( cacheDir ), "%sOculus\\%s\\cache\\", lpTempPathBuffer, executableName );
	
			char cacheDirCanonical[ ovrPathUtils::URI_MAX_PATH ];
			ovrPathUtils::FixSlashesForUri( cacheDir, cacheDirCanonical, sizeof( cacheDirCanonical ) );

			MakePath( cacheDirCanonical, permissionFlags_t( PERMISSION_WRITE ) | PERMISSION_READ );

			return String( cacheDirCanonical );
		}
		else
		{
			LOG( "Unable to locate cache directory" );
			return String();
		}
	}
	else
	{
		char curWorkingDir[ MAX_PATH ];
		_getcwd( curWorkingDir, sizeof( curWorkingDir ) );

		return String( curWorkingDir );
	}
#else
	return String();
#endif
}

OvrStoragePaths::OvrStoragePaths( JNIEnv * jni, jobject activityObj )
{
	ovrJava java;
	java.Env = jni;
	java.ActivityObject = activityObj;
	java.Vm = NULL;

	VrActivityClass = ovr_GetGlobalClassReference( java.Env, java.ActivityObject, "com/oculus/vrappframework/VrActivity" );
	InternalCacheMemoryID = ovr_GetStaticMethodID( java.Env, VrActivityClass, "getInternalCacheMemoryInBytes", "(Landroid/app/Activity;)J" );

	for ( int i = 0; i < EST_COUNT; i++ )
	{
		for ( int j = 0; j < EFT_COUNT; j++ )
		{
			StorageFolderPaths[i][j] = GetDir( static_cast<ovrStorageType>( i ), static_cast<ovrFolderType>( j ), VrActivityClass, &java );
		}
	}

	for ( int i = 0; i < EST_COUNT; i++ )
	{
		for ( int j = 0; j < EFT_COUNT; j++ )
		{
			LOG( "StorageFolderPaths[%i][%i] = %s", i, j, StorageFolderPaths[i][j].ToCStr() );
		}
	}
}

void OvrStoragePaths::PushBackSearchPathIfValid( ovrStorageType toStorage, ovrFolderType toFolder, const char * subfolder, Array<String> & searchPaths ) const
{
	PushBackSearchPathIfValidPermission( toStorage, toFolder, subfolder, permissionFlags_t( PERMISSION_READ ), searchPaths );
}

void OvrStoragePaths::PushBackSearchPathIfValidPermission( ovrStorageType toStorage, ovrFolderType toFolder, const char * subfolder, permissionFlags_t permission, Array<String> & searchPaths ) const
{
	String checkPath;
	if ( GetPathIfValidPermission( toStorage, toFolder, subfolder, permission, checkPath ) )
	{
		searchPaths.PushBack( checkPath );
	}
}

bool OvrStoragePaths::GetPathIfValidPermission( ovrStorageType toStorage, ovrFolderType toFolder, const char * subfolder, permissionFlags_t permission, String & outPath ) const
{
	if ( StorageFolderPaths[ toStorage ][ toFolder ].GetSize() > 0 )
	{
		String checkPath = StorageFolderPaths[ toStorage ][ toFolder ] + subfolder;
		if ( HasPermission( checkPath.ToCStr(), permission ) )
		{
			outPath = checkPath;
			return true;
		}
		else
		{
			WARN( "Failed to get permission for %s storage in %s folder ", StorageName[toStorage], FolderName[toFolder] );
		}
	}
	else
	{
		WARN( "Path not found for %s storage in %s folder ", StorageName[toStorage], FolderName[toFolder] );
	}
	return false;
}

bool OvrStoragePaths::HasStoragePath( const ovrStorageType toStorage, const ovrFolderType toFolder ) const
{
	return ( StorageFolderPaths[ toStorage ][ toFolder ].GetSize() > 0 );
}

long long OvrStoragePaths::GetAvailableInternalMemoryInBytes( JNIEnv * jni, jobject activityObj ) const
{
#if defined( OVR_OS_ANDROID )
	return (long long )( jni->CallStaticLongMethod( VrActivityClass, InternalCacheMemoryID, activityObj ) );
#else
	char curWorkingDir[ MAX_PATH ];
	_getcwd( curWorkingDir, sizeof( curWorkingDir ) );

	char curDrive[ MAX_PATH ];
	OVR::OVR_strcpy( curDrive, sizeof( curDrive ), "C:\\" );
	curDrive[ 0 ] = curWorkingDir[ 0 ];

	ULARGE_INTEGER freeBytesAvailable;
	if ( GetDiskFreeSpaceEx( curDrive, &freeBytesAvailable, NULL, NULL ) )
	{
		return freeBytesAvailable.QuadPart;
	}

	return 0;
#endif
}

String GetFullPath( const Array<String>& searchPaths, const String & relativePath )
{
	if ( FileExists( relativePath.ToCStr() ) )
	{
		return relativePath;
	}

	const int numSearchPaths = searchPaths.GetSizeI();
	for ( int index = 0; index < numSearchPaths; ++index )
	{
		const String fullPath = searchPaths.At( index ) + String( relativePath );
		if ( FileExists( fullPath.ToCStr() ) )
		{
			return fullPath;
		}
	}

	return String();
}

bool GetFullPath( const Array<String>& searchPaths, char const * relativePath, char * outPath, const int outMaxLen )
{
	OVR_ASSERT( outPath != NULL && outMaxLen >= 1 );

	if ( FileExists( relativePath ) )
	{
		OVR_sprintf( outPath, OVR_strlen( relativePath ) + 1, "%s", relativePath );
		return true;
	}

	for ( int i = 0; i < searchPaths.GetSizeI(); ++i )
	{
		OVR_sprintf( outPath, outMaxLen, "%s%s", searchPaths[i].ToCStr(), relativePath );
		if ( FileExists( outPath ) )
		{
			return true;	// outpath is now set to the full path
		}
	}
	// just return the relative path if we never found the file
	OVR_sprintf( outPath, outMaxLen, "%s", relativePath );
	return false;
}

bool GetFullPath( const Array<String>& searchPaths, char const * relativePath, String & outPath )
{
	char largePath[1024];
	bool result = GetFullPath( searchPaths, relativePath, largePath, sizeof( largePath ) );
	if ( result )
	{
		outPath = largePath;
	}
	return result;
}

bool ToRelativePath( const Array<String>& searchPaths, char const * fullPath, char * outPath, const int outMaxLen )
{
	// check if the path starts with any of the search paths
	const int n = searchPaths.GetSizeI();
	for ( int i = 0; i < n; ++i )
	{
		char const * path = searchPaths[i].ToCStr();
		if ( strstr( fullPath, path ) == fullPath )
		{
			size_t len = OVR_strlen( path );
			OVR_sprintf( outPath, outMaxLen, "%s", fullPath + len );
			return true;
		}
	}
	OVR_sprintf( outPath, outMaxLen, "%s", fullPath );
	return false;
}

bool ToRelativePath( const Array<String>& searchPaths, char const * fullPath, String & outPath )
{
	char largePath[1024];
	bool result = ToRelativePath( searchPaths, fullPath, largePath, sizeof( largePath ) );
	outPath = largePath;
	return result;
}

//==============================================================================================
// ovrPathUtils
//==============================================================================================

const char * ovrPathUtils::WIN_PATH_SEPARATOR_STRING = "\\";
const char * ovrPathUtils::NIX_PATH_SEPARATOR_STRING = "/";
const char * ovrPathUtils::URI_PATH_SEPARATOR_STRING = "/";

static uint32_t DecodeNextChar( char const * p, intptr_t & offset )
{
	char const * t = p + offset;
	uint32_t ch = UTF8Util::DecodeNextChar( &t );
	offset = t - p;
	return ch;
}

// This is false only during unit tests because during the tests we want to validate that the
// truncation happens correctly without asserting on a truncate. In all other cases we want
// to get an assert on a overflow / truncate so that we have a chance of catching it in
// debug builds outside of test code.
static bool AssertOnOverflow = true;	

static bool EncodeChar( char * p, size_t const & maxOffset, intptr_t & offset, uint32_t ch )
{
	// test for buffer overflow by encoding to a temp buffer and seeing how far the offset moved
	char temp[6];
	intptr_t tempOfs = 0;
	UTF8Util::EncodeChar( temp, &tempOfs, ch );
	if ( static_cast< intptr_t >( maxOffset ) - offset <= tempOfs )
	{
		// just encode a null byte at the current offset
		OVR_ASSERT( !AssertOnOverflow );
		p[offset] = '\0';
		offset++;
		return false;
	}
	UTF8Util::EncodeChar( p, &offset, ch );
	return true;
}

bool DecodePrevChar( char const * p, intptr_t & offset, uint32_t & ch )
{
	if ( offset <= 0 )
	{
		ch = '\0';
		return false;
	}
	for ( int i = 1; i < 6; ++i )
	{
		intptr_t ofs = offset - i;
		if ( ofs < 0 )
		{
			ch = '\0';
			return false;
		}
		char t = *( p + ofs );
		if ( ( t & 0x80 ) == 0 )
		{
			// normal ascii char
			offset = ofs;
			ch = t;
			return true;
		}
		// if not a low-ascii, the the byte must start with 11 if it's a leading character byte, or 10 otherwise
		else if ( ( t & 0xE0 ) == 0xC0 || ( t & 0xF0 ) == 0xE0 || ( t & 0xF8 ) == 0xF0 || ( t & 0xFC ) == 0xF8 || ( t  & 0xFE ) == 0xFC ) 
		{
			// leading byte, decode from here
			char const * tp = p + ofs;
			ch = UTF8Util::DecodeNextChar( &tp );
			offset = ofs;
			return true;
		}
		else if ( ( t & 0xC0 ) != 0x80 )
		{
			// this is not a UTF8 encoding
			OVR_ASSERT( false );
			return false;
		}
	}
	return false;
}

void ovrPathUtils::TestDecodeNext( char const * inStr, char * outStr, size_t const outStrSize )
{
	OVR_ASSERT( inStr != NULL && outStr != NULL && outStrSize > 0 );
	intptr_t inOfs = 0;
	intptr_t outOfs = 0;
	for ( uint32_t ch = DecodeNextChar( inStr, inOfs );
		ch != 0 && outOfs < static_cast< intptr_t >( outStrSize - 1 );
		ch = DecodeNextChar( inStr, inOfs ) )
	{
		EncodeChar( outStr, outStrSize, outOfs, ch );
	}
	EncodeChar( outStr, outStrSize, outOfs, '\0' );
}

void ovrPathUtils::ReverseString( char const * inStr, char * outStr, size_t const outStrSize )
{
	OVR_ASSERT( inStr != NULL && outStr != NULL && outStrSize > 0 );
	size_t len = OVR_strlen( inStr );
	intptr_t inOfs = static_cast< intptr_t >( len );
	intptr_t outOfs = 0;
	for ( ; ; )
	{
		uint32_t ch;
		if ( !DecodePrevChar( inStr, inOfs, ch ) )
		{
			EncodeChar( outStr, outStrSize, outOfs, '\0' );
			break;
		}
		EncodeChar( outStr, outStrSize, outOfs, ch );
	}
}

static bool ReverseStringAndTest( char const * inStr, char * outStr, size_t const outStrSize )
{
	ovrPathUtils::ReverseString( inStr, outStr, outStrSize );

	OVR_ASSERT( UTF8Util::GetLength( outStr ) == UTF8Util::GetLength( inStr ) );

	intptr_t io = 0;
	intptr_t oo = static_cast< intptr_t >( OVR_strlen( outStr ) );

	for ( ; ; )
	{
		uint32_t inCh = DecodeNextChar( inStr, io );
		if ( inCh == '\0' )
		{
			break;
		}
		uint32_t outCh;
		if ( !DecodePrevChar( outStr, oo, outCh ) )
		{
			OVR_ASSERT( false );
			return false;
		}
		if ( inCh != outCh )
		{
			LOG( "Failed!" );
			return false;
		}
	}
	return true;
}

void ovrPathUtils::ReplaceCharsInString( uint32_t const toReplace, uint32_t const replaceWith, char const * inStr, 
		char * outStr, size_t const outStrSize )
{
	OVR_ASSERT( inStr != NULL && outStr != NULL && outStrSize > 0 );
	outStr[0] = '\0';

	char const * p = inStr;
	intptr_t offset = 0;
	for ( uint32_t ch = UTF8Util::DecodeNextChar( &p ); 
		ch != '\0' && static_cast< size_t >( offset ) < outStrSize; 
		ch = UTF8Util::DecodeNextChar( &p ) )
	{
		if ( ch == toReplace )
		{
			EncodeChar( outStr, outStrSize, offset, replaceWith );
		}
		else
		{
			EncodeChar( outStr, outStrSize, offset, ch );
		}
	}
	EncodeChar( outStr, outStrSize, offset, '\0' );
}

void ovrPathUtils::FixSlashesForUri( char const * inPath, char * outPath, size_t const outPathSize )
{
	ReplaceCharsInString( WIN_PATH_SEPARATOR, URI_PATH_SEPARATOR, inPath, outPath, outPathSize );
}

void ovrPathUtils::FixSlashesForWindows( char const * inPath, char * outPath, size_t const outPathSize )
{
	ReplaceCharsInString( URI_PATH_SEPARATOR, WIN_PATH_SEPARATOR, inPath, outPath, outPathSize );
}

void ovrPathUtils::FixSlashesForUriInPlace( char * path )
{
	intptr_t offset = 0;
	for ( uint32_t ch = DecodeNextChar( path, offset ); 
		ch != '\0'; 
		ch = DecodeNextChar( path, offset ) )
	{
		if ( ch == WIN_PATH_SEPARATOR )
		{
			path[offset - 1] = URI_PATH_SEPARATOR;
		}
	}
}

void ovrPathUtils::FixSlashesForWindowsInPlace( char * path )
{
	intptr_t offset = 0;
	for ( uint32_t ch = DecodeNextChar( path, offset ); 
		ch != '\0'; 
		ch = DecodeNextChar( path, offset ) )
	{
		if ( ch == NIX_PATH_SEPARATOR )
		{
			path[offset - 1] = WIN_PATH_SEPARATOR;
		}
	}
}

bool ovrPathUtils::UriPathStartsWithDriveLetter( char const * uriPath )
{
	if ( uriPath == NULL || uriPath[0] == '\0' )
	{
		return false;
	}
	if ( uriPath[0] != '/' )
	{
		return false;
	}
	size_t len = UTF8Util::GetLength( uriPath );
	if ( len < 3 )
	{
		return false;
	}
	if ( uriPath[0] != '/' || uriPath[2] != ':' )
	{
		return false;
	}
	return true;
}

// this takes a pointer to a buffer containing a URI path as input and outputs a path that is safe
// to use on Windows. In particular, if the path starts with the form "/C:", then the returned path
// skips past the '/' and starts with the drive letter.
const char * ovrPathUtils::SafePathFromUriPath( char const * uriPath )
{
	if ( UriPathStartsWithDriveLetter( uriPath ) ) 
	{
		return uriPath + 1;
	}
	return uriPath;
}

bool ovrPathUtils::CollapsePath( char const * inPath, char * outPath, size_t const outPathSize )
{
	OVR_ASSERT( outPath != NULL && outPathSize > 2 );

	outPath[0] = '\0';

	// Emit characters to the out string
	// When we get to a ., check if the previous character was . and the prev-prev character was a path separator.
	// If so, backup in the output until we find the second slash
	// or we pass the start of the output, in which case the input path was ill-formed
	intptr_t outOfs = 0;
	intptr_t inOfs = 0;
	uint32_t ch = 0xffffffff;
	uint32_t prevCh = 0xffffffff;
	uint32_t prevPrevCh = 0xffffffff;
	for ( ; ; )
	{
		prevPrevCh = prevCh;
		prevCh = ch;
		ch = DecodeNextChar( inPath, inOfs );
		if ( ch == '\0' )
		{
			if ( !EncodeChar( outPath, outPathSize, outOfs, '\0' ) )
			{
				return false;
			}
			break;
		}

		if ( ch == '.' && prevCh == '.' && ( prevPrevCh == NIX_PATH_SEPARATOR || prevPrevCh == WIN_PATH_SEPARATOR ) )
		{
			// backup until we come to the second path separator
			int separatorCount = 0;
			for ( ; ; )
			{
				uint32_t pch = 0;
				if ( !DecodePrevChar( outPath, outOfs, pch ) )
				{
					// ill-formed path or not a valid UTF-8 encoding
					outPath[0] = '\0';
					return false;
				}
				if ( pch == NIX_PATH_SEPARATOR || pch == WIN_PATH_SEPARATOR )
				{
					separatorCount++;
					if ( separatorCount == 2 )
					{
						break;
					}
				}
			}
		}
		else
		{
			if ( !EncodeChar( outPath, outPathSize, outOfs, ch ) )
			{
				return false;
			}
		}
	}
	return true;
}

bool ovrPathUtils::AppendUriPath( char * inPath, size_t const inPathSize, char const * append )
{
	OVR_ASSERT( inPath != NULL );

	// make sure both paths are using correct slashes
	char appendCanonical[URI_MAX_PATH];
	FixSlashesForUri( append, appendCanonical, sizeof( appendCanonical ) );
	FixSlashesForUriInPlace( inPath );

	intptr_t ofs = OVR_strlen( inPath );
	if ( ofs == 0 )
	{
		OVR_strcpy( inPath, inPathSize, append );
		return true;
	}

	uint32_t ch;
	if ( !DecodePrevChar( inPath, ofs, ch ) )
	{
		// we already checked for 0-length inPath, so this must be an invalid UTF-8 encoding
		OVR_ASSERT( false );
		return false;
	}
	if ( ch != URI_PATH_SEPARATOR )
	{
		OVR_strcat( inPath, inPathSize, URI_PATH_SEPARATOR_STRING );
	}
	// skip past any starting separators
	intptr_t appendOfs = 0;
	char const * appendCanonicalStart = &appendCanonical[0];
	for ( ; ; )
	{
		uint32_t appendCh = DecodeNextChar( appendCanonical, appendOfs );
		if ( appendCh != NIX_PATH_SEPARATOR && appendCh != WIN_PATH_SEPARATOR && appendCh != URI_PATH_SEPARATOR )
		{
			break;
		}
		appendCanonicalStart = appendCanonical + appendOfs;
	}
	OVR_strcat( inPath, inPathSize, appendCanonicalStart );
	return true;
}

bool ovrPathUtils::AppendUriPath( char const * inPath, char const * appendPath, char * outPath, size_t const outPathSize )
{
	if ( inPath == NULL || outPath == NULL || appendPath == NULL || outPathSize < 2 )
	{
		OVR_ASSERT( inPath != NULL && outPath != NULL && appendPath != NULL && outPathSize > 1 );
		return false;
	}
	intptr_t inOfs = 0;
	intptr_t outOfs = 0;
	uint32_t lastCh = 0xffffffff;
	uint32_t ch = 0xffffffff;
	for ( ; ; )
	{
		lastCh = ch;
		ch = DecodeNextChar( inPath, inOfs );
		if ( ch == '\0' )
		{
			break;
		}
		if ( !EncodeChar( outPath, outPathSize, outOfs, ch ) )
		{
			return false;
		}
	}

	// ensure there's always a path separator after inPath
	if ( lastCh != WIN_PATH_SEPARATOR && lastCh != NIX_PATH_SEPARATOR && lastCh != URI_PATH_SEPARATOR )
	{
		// emit a separator
		if ( !EncodeChar( outPath, outPathSize, outOfs, URI_PATH_SEPARATOR ) )
		{
			return false;	// buffer overflow
		}
	}

	// skip past any path separators at the start of append path
	intptr_t appendOfs = 0;
	char const * appendPathStart = &appendPath[0];
	for ( ; ; )
	{
		ch = DecodeNextChar( appendPath, appendOfs );
		if ( ch != WIN_PATH_SEPARATOR && ch != NIX_PATH_SEPARATOR && ch != URI_PATH_SEPARATOR )
		{
			break;
		}
		appendPathStart = appendPath + appendOfs;
	}

	appendOfs = 0;
	for ( ; ; )
	{
		ch = DecodeNextChar( appendPathStart, appendOfs );
		if ( !EncodeChar( outPath, outPathSize, outOfs, ch ) )
		{
			return false;
		}
		if ( ch == 0 )
		{
			return true;
		}
	}
}

void ovrPathUtils::StripFilename( char const * inPath, char * outPath, size_t const outPathSize )
{
	OVR_ASSERT( inPath != NULL );
	if ( inPath[0] == '\0' )
	{
		OVR_strcpy( outPath, outPathSize, inPath );
		return;
	}

	intptr_t inOfs = OVR_strlen( inPath );
	for ( ; ; )
	{
		uint32_t ch;
		if ( !DecodePrevChar( inPath, inOfs, ch ) )
		{
			// invalid UTF-8 encoding
			OVR_ASSERT( false );
			break;
		}
		if ( ch == WIN_PATH_SEPARATOR || ch == NIX_PATH_SEPARATOR || ch == URI_PATH_SEPARATOR )
		{
			OVR_strncpy( outPath, outPathSize, inPath, inOfs + 1 );
			return;
		}
	}
	// never hit a path separator so copy the entire thing
	OVR_strcpy( outPath, outPathSize, inPath );
}


void ovrPathUtils::DoUnitTests()
{
	AssertOnOverflow = false;

	char testPath[512];
	if ( CollapsePath( "/test/../../path", testPath, sizeof( testPath ) ) )
	{
		OVR_ASSERT( false );
		WARN( "CollapsePath() failed to detect ill-formed path!" );
	}
	bool result = CollapsePath( "/test/../path", testPath, sizeof( testPath ) );
	if ( !result || OVR_strcmp( testPath, "/path" ) != 0 )
	{
		OVR_ASSERT( false );
		WARN( "CollapsePath() failed!" );
	}

	if ( !ReverseStringAndTest( "0123456789", testPath, sizeof( testPath ) ) )
	{
		OVR_ASSERT( false );
		WARN( "ReverseString() failed on ASCII text!" );
	}
	// test walking backwards through UTF-8 strings
	// VS won't reliably encode UTF-8 so we have to use a byte array
	uint8_t koreanText[] = { 0xEA, 0xB3, 0xA0, 0xEB, 0x8C, 0x80, 0x20, 0xEC, 0x9C, 0xA0, 0xEC, 0xA0, 0x81, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00 };
	if ( !ReverseStringAndTest( (char*)&koreanText[0], testPath, sizeof( testPath ) ) )
	{
		OVR_ASSERT( false );
		WARN( "ReverseString() failed on UTF-8 text!" );
	}

	AppendUriPath( "file://foo/", "bar/", testPath, sizeof( testPath ) );
	OVR_ASSERT( OVR_strcmp( testPath, "file://foo/bar/" ) == 0 );

	memset( testPath, 0, sizeof( testPath ) );
	AppendUriPath( "file://foo/", "/bar/", testPath, sizeof( testPath ) );
	OVR_ASSERT( OVR_strcmp( testPath, "file://foo/bar/" ) == 0 );

	memset( testPath, 0, sizeof( testPath ) );
	AppendUriPath( "file://foo", "/bar/", testPath, sizeof( testPath ) );
	OVR_ASSERT( OVR_strcmp( testPath, "file://foo/bar/" ) == 0 );

	memset( testPath, 0, sizeof( testPath ) );
	AppendUriPath( "file://foo", "bar/", testPath, sizeof( testPath ) );
	OVR_ASSERT( OVR_strcmp( testPath, "file://foo/bar/" ) == 0 );

	char shortPath[12];
	if ( AppendUriPath( "file://foo/", "bar/", shortPath, sizeof( shortPath ) ) )
	{
		OVR_ASSERT( false );
		WARN( "AppendUri() should have failed!" );
	}
	OVR_ASSERT( OVR_strcmp( shortPath, "file://foo/" ) == 0 );

	if ( AppendUriPath( "file://foo/", "/bar/", shortPath, sizeof( shortPath ) ) )
	{
		OVR_ASSERT( false );
		WARN( "AppendUri() should have failed!" );
	}
	OVR_ASSERT( OVR_strcmp( shortPath, "file://foo/" ) == 0 );

	OVR_strcpy( testPath, sizeof( testPath ), "file://foo/" );
	AppendUriPath( testPath, sizeof( testPath ), "bar/" );
	OVR_ASSERT( OVR_strcmp( testPath, "file://foo/bar/" ) == 0 );

	OVR_strcpy( testPath, sizeof( testPath ), "file://foo/" );
	AppendUriPath( testPath, sizeof( testPath ), "/bar/" );
	OVR_ASSERT( OVR_strcmp( testPath, "file://foo/bar/" ) == 0 );

	OVR_strcpy( testPath, sizeof( testPath ), "file://foo" );
	AppendUriPath( testPath, sizeof( testPath ), "/bar/" );
	OVR_ASSERT( OVR_strcmp( testPath, "file://foo/bar/" ) == 0 );

	OVR_strcpy( testPath, sizeof( testPath ), "file://foo" );
	AppendUriPath( testPath, sizeof( testPath ), "bar/" );
	OVR_ASSERT( OVR_strcmp( testPath, "file://foo/bar/" ) == 0 );

	AssertOnOverflow = true;
}

} // namespace OVR
