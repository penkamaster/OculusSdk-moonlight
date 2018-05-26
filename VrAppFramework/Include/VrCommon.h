
/************************************************************************************

Filename    :   VrCommon.h
Content     :   Unorganized common code
Created     :   Septembet 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_VrCommon_h
#define OVR_VrCommon_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_StringHash.h"
#include "Kernel/OVR_BitFlags.h"
#include "PathUtils.h"

#ifdef OVR_OS_WIN32
#define S_IRUSR 1
#define S_IWUSR 2

#define R_OK 4
#define W_OK 2
#define F_OK 0
#endif

namespace OVR {

// Debug tool
void LogMatrix( const char * title, const Matrix4f & m );

inline Vector3f GetViewMatrixPosition( Matrix4f const & m )
{
#if 1
	return m.Inverted().GetTranslation();
#else
	// This is much cheaper if the view matrix is a pure rotation plus translation.
	return Vector3f(	m.M[0][0] * m.M[0][3] + m.M[1][0] * m.M[1][3] + m.M[2][0] * m.M[2][3],
						m.M[0][1] * m.M[0][3] + m.M[1][1] * m.M[1][3] + m.M[2][1] * m.M[2][3],
						m.M[0][2] * m.M[0][3] + m.M[1][2] * m.M[1][3] + m.M[2][2] * m.M[2][3] );
#endif
}

inline Vector3f GetViewMatrixForward( Matrix4f const & m )
{
	return Vector3f( -m.M[2][0], -m.M[2][1], -m.M[2][2] ).Normalized();
}

inline Vector3f GetViewMatrixUp( Matrix4f const & m )
{
	return Vector3f( -m.M[1][0], -m.M[1][1], -m.M[1][2] ).Normalized();
}

inline Vector3f GetViewMatrixLeft( Matrix4f const & m )
{
	return Vector3f( -m.M[0][0], -m.M[0][1], -m.M[0][2] ).Normalized();
}

// Returns true if the folder or file has the specified permission
bool HasPermission( const char * fileOrDirName, const permissionFlags_t flags );

// Returns true if the file exists
bool FileExists( const char * filename );
// Returns true if folderName is a folder, false if it is a file or something else (symbolic link) or doesn't exist
bool IsFolder( const char * folderName );

// Returns true if ext is the end of fileName
bool MatchesExtension( const char * fileName, const char * ext );

void SortStringArray( Array<String> & strings );

void AppendPath( String & startPath, const char * pathToAppend );

StringHash< String > RelativeDirectoryFileList( const Array< String > & searchPaths, const char * RelativeDirPath );

// DirPath should by a directory with a trailing slash.
// Returns all files in the directory, already prepended by root.
// Subdirectories will have a trailing slash.
// All files and directories that start with . are skipped.
Array<String> DirectoryFileList( const char * DirPath );

// Returns the last token in path s with slashes and file extension stripped
String ExtractFileBase( const String & s );

// Returns the filename with extension from a passed in path
String ExtractFile( const String & s );

// Returns the directory name before the fileName - stripping out parent directories and file
String ExtractDirectory( const String & s );

// Creates all the intermediate directories if they don't exist
void MakePath( const char * dirPath, permissionFlags_t permissions );

// Returns true if head equals check plus zero or more characters.
bool MatchesHead( const char * head, const char * check );

float LinearRangeMapFloat( float inValue, float inStart, float inEnd, float outStart, float outEnd );

}	// namespace OVR

#endif	// OVR_VrCommon_h
