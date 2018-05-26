/************************************************************************************

Filename    :   PathUtils.h
Content     :
Created     :   November 26, 2014
Authors     :   Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_PathUtils_h
#define OVR_PathUtils_h

#include <inttypes.h>
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_BitFlags.h"

// FIXME: most of VrCommon.h and all of PathUtils.h should be merged into a FileUtils.h

namespace OVR {

enum ovrPermission {
	PERMISSION_READ,
	PERMISSION_WRITE,
	PERMISSION_EXECUTE
};
typedef BitFlagsT< ovrPermission > permissionFlags_t;

enum ovrStorageType
{
	// By default data here is private and other apps shouldn't be able to access data from here
	// Path => "/data/data/", in Note 4 this is 24.67GB
	EST_INTERNAL_STORAGE = 0,

	// Also known as emulated internal storage, as this is part of phone memory( that can't be removed ) which is emulated as external storage
	// in Note 4 this is = 24.64GB, with WRITE_EXTERNAL_STORAGE permission can write anywhere in this storage
	// Path => "/storage/emulated/0" or "/sdcard",
	EST_PRIMARY_EXTERNAL_STORAGE,

	// Path => "/storage/extSdCard"
	// Can only write to app specific folder - /storage/extSdCard/Android/obb/<app>
	EST_SECONDARY_EXTERNAL_STORAGE,

	EST_COUNT
};

enum ovrFolderType
{
	// Root folder, for example:
	//		internal 			=> "/data"
	//		primary external 	=> "/storage/emulated/0"
	//		secondary external 	=> "/storage/extSdCard"
	EFT_ROOT = 0,

	// Files folder
	EFT_FILES,

	// Cache folder, data in this folder can be flushed by OS when it needs more memory.
	EFT_CACHE,

	EFT_COUNT
};

class OvrStoragePaths
{
public:
				OvrStoragePaths( JNIEnv * jni, jobject activityObj );
	void		PushBackSearchPathIfValid( ovrStorageType toStorage, ovrFolderType toFolder, const char * subfolder, Array<String> & searchPaths ) const;
	void		PushBackSearchPathIfValidPermission( ovrStorageType toStorage, ovrFolderType toFolder, const char * subfolder, permissionFlags_t permission, Array<String> & searchPaths ) const;
	bool		GetPathIfValidPermission( ovrStorageType toStorage, ovrFolderType toFolder, const char * subfolder, permissionFlags_t permission, String & outPath ) const;
	bool		HasStoragePath( const ovrStorageType toStorage, const ovrFolderType toFolder ) const;
	long long 	GetAvailableInternalMemoryInBytes( JNIEnv * jni, jobject activityObj ) const;

private:
	// contains all the folder paths for future reference
	String 			StorageFolderPaths[EST_COUNT][EFT_COUNT];
	jclass			VrActivityClass;
	jmethodID		InternalCacheMemoryID;
};

String	GetFullPath		( const Array< String > & searchPaths, const String & relativePath );

// Return false if it fails to find the relativePath in any of the search locations
bool	GetFullPath		( const Array< String > & searchPaths, char const * relativePath, 	char * outPath, 	const int outMaxLen );
bool	GetFullPath		( const Array< String > & searchPaths, char const * relativePath, 	String & outPath 						);

bool	ToRelativePath	( const Array< String > & searchPaths, char const * fullPath, 		char * outPath, 	const int outMaxLen );
bool	ToRelativePath	( const Array< String > & searchPaths, char const * fullPath, 		String & outPath 						);

//==============================================================
// ovrPathUtils
// UTF8-safe path utilities. Eventually this should be exposed.
// This class is only functional, stores no data, and operates on char buffers so that
// it can be wrapped by string classes.
class ovrPathUtils
{
public:
	static const char WIN_PATH_SEPARATOR = '\\';
	static const char NIX_PATH_SEPARATOR = '/';
	static const char URI_PATH_SEPARATOR = '/';

	static const char *	WIN_PATH_SEPARATOR_STRING;
	static const char * NIX_PATH_SEPARATOR_STRING;
	static const char * URI_PATH_SEPARATOR_STRING;

	static const int	URI_MAX_PATH = 1024;

	static void			ReplaceCharsInString( uint32_t const toReplace, uint32_t const replaceWith, 
								char const * inStr, char * outStr, size_t const outStrSize );
	static void			FixSlashesForUri( char const * inPath, char * outPath, size_t const outPathSize );
	static void			FixSlashesForWindows( char const * inPath, char * outPath, size_t const outPathSize );

	static void			FixSlashesForUriInPlace( char * path );
	static void			FixSlashesForWindowsInPlace( char * path );

	static void			StripFilename( char const * inPath, char * outPath, size_t const outPathSize );

	static bool			AppendUriPath( char * inPath, size_t const inPathSize, char const * append );
	static bool			AppendUriPath( char const * inPath, char const * append, char * outPath, size_t const outPathSize );

	// Returns true if the path is a Uri path (starts with a '/') and a Windows drive letter pattern (C:).
	// On Windows machines a URI containing a drive letter must be encoded as:
	// file:///C:/SomeFolder/SomeFile.txt
	// Which gives us a URI path of: /C:/SomeFolder/SomeFile.txt
	// In order to be a valid Windows path, the leading / must be stripped or skipped (and technically
	// the forward slashes must be converted to back slashes, but the Win32 API is very tolerant of this
	// and in most cases handles / as if it were \.
	static bool			UriPathStartsWithDriveLetter( char const * uriPath );

	// Takes a pointer to a buffer containing a URI path as input and outputs a path that is safe
	// to use on Windows. In particular, if the path starts with the form "/C:", then the returned path
	// skips past the '/' and starts with the drive letter.
	static const char * SafePathFromUriPath( char const * uriPath );

	// Collapses a path, removing navigation elements such as ..
	// Returns false if the path is ill-formed.
	static bool			CollapsePath( char const * inPath, char * outPath, size_t const outPathSize );

	static void			TestDecodeNext( char const * inStr, char * outStr, size_t const outStrSize );
	static void			ReverseString( char const * inStr, char * outStr, size_t const outStrSize );

	static void			DoUnitTests();
};

} // namespace OVR

#endif // OVR_PathUtils_h

