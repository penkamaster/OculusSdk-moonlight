/************************************************************************************

Filename    :   VrCommon.cpp
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrCommon.h"

#if defined( OVR_OS_ANDROID )
#include <dirent.h>
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "Kernel/OVR_MemBuffer.h"
#include "Kernel/OVR_LogUtils.h"

#if defined( OVR_OS_WIN32 )
typedef unsigned short mode_t;
#include <io.h>
#endif

namespace OVR {

void LogMatrix( const char * title, const Matrix4f & m )
{
	LOG( "%s:", title );
	for ( int i = 0; i < 4; i++ )
	{
		LOG("%6.3f %6.3f %6.3f %6.3f", m.M[i][0], m.M[i][1], m.M[i][2], m.M[i][3] );
	}
}

static int StringCompareNoCase( const void *a, const void * b )
{
	const String *sa = ( String * )a;
	const String *sb = ( String * )b;
	return sa->CompareNoCase( *sb );
}

void SortStringArray( Array<String> & strings )
{
	if ( strings.GetSize() > 1 )
	{
		qsort( ( void * )&strings[ 0 ], strings.GetSize(), sizeof( String ), StringCompareNoCase );
	}
}

// if pathToAppend is an empty string, this just adds a slash
void AppendPath( String & startPath, const char * pathToAppend )
{
	int const len = startPath.GetLengthI();
	if ( len == 0 )
	{
		startPath = pathToAppend;
		return;
	}
	uint32_t lastCh = startPath[len - 1];
	if ( lastCh != '/' && lastCh != '\\' )
	{
		// always append the linux path, assuming it will be corrected elsewhere if necessary for Windows
		startPath += '/';
	}
	startPath += pathToAppend;
}

// DirPath should by a directory with a trailing slash.
// Returns all files in all search paths, as unique relative paths.
// Subdirectories will have a trailing slash.
// All files and directories that start with . are skipped.
StringHash< String > RelativeDirectoryFileList( const Array< String > & searchPaths, const char * RelativeDirPath )
{
	//Check each of the mirrors in searchPaths and build up a list of unique strings
	StringHash< String >	uniqueStrings;

	const int numSearchPaths = searchPaths.GetSizeI();
	for ( int index = 0; index < numSearchPaths; ++index )
	{
#if defined( OVR_OS_WIN32 )
		String fullPath( searchPaths[index] );
		AppendPath( fullPath, RelativeDirPath );
		AppendPath( fullPath, "*.*" );
		__finddata64_t fileInfo;
		intptr_t handle = _findfirst64( fullPath.ToCStr(), &fileInfo );
		if ( handle == -1 )
		{
			continue;
		}

		do
		{
			if ( OVR_stricmp( fileInfo.name, "." ) == 0 || OVR_stricmp( fileInfo.name, ".." ) == 0 )
			{
				continue;
			}
			String s( RelativeDirPath );
			AppendPath( s, fileInfo.name );
			if ( ( fileInfo.attrib & _A_SUBDIR ) != 0 )
			{
				AppendPath( s, "" );	// this will add a /
			}
			uniqueStrings.SetCaseInsensitive( s, s );
		} while( _findnext64( handle, &fileInfo ) != -1 );

		_findclose( handle );
#else
		const String fullPath = searchPaths[index] + String( RelativeDirPath );
		DIR * dir = opendir( fullPath.ToCStr() );
		if ( dir != NULL )
		{
			struct dirent * entry;
			while ( ( entry = readdir( dir ) ) != NULL )
			{
				if ( entry->d_name[ 0 ] == '.' )
				{
					continue;
				}
				if ( entry->d_type == DT_DIR )
				{
					String s( RelativeDirPath );
					s += entry->d_name;
					s += "/";
					uniqueStrings.SetCaseInsensitive( s, s );
				}
				else if ( entry->d_type == DT_REG )
				{
					String s( RelativeDirPath );
					s += entry->d_name;
					uniqueStrings.SetCaseInsensitive( s, s );
				}
			}
			closedir( dir );
		}
#endif
	}

	return uniqueStrings;
}

// DirPath should by a directory with a trailing slash.
// Returns all files in the directory, already prepended by root.
// Subdirectories will have a trailing slash.
// All files and directories that start with . are skipped.
Array<String> DirectoryFileList( const char * dirPath )
{
	Array<String>	strings;
#if defined( OVR_OS_WIN32 )
	String fullPath( dirPath );
	AppendPath( fullPath, "*.*" );
	__finddata64_t fileInfo;
	intptr_t handle = _findfirst64( fullPath.ToCStr(), &fileInfo );
	if ( handle != -1 )
	{
		do
		{
			if ( OVR_stricmp( fileInfo.name, "." ) == 0 || OVR_stricmp( fileInfo.name, ".." ) == 0 )
			{
				continue;
			}
			String s( dirPath );
			AppendPath( s, fileInfo.name );
			if ( ( fileInfo.attrib & _A_SUBDIR ) != 0 )
			{
				AppendPath( s, "" ); // this will add a /
			}
			strings.PushBack( s );
		} while( _findnext64( handle, &fileInfo ) != -1 );

		_findclose( handle );
	}
#else // POSIX
	DIR * dir = opendir( dirPath );
	if ( dir != NULL )
	{
		struct dirent * entry;
		while ( ( entry = readdir( dir ) ) != NULL )
		{
			if ( entry->d_name[ 0 ] == '.' )
			{
				continue;
			}
			if ( entry->d_type == DT_DIR )
			{
				String s( dirPath );
				s += entry->d_name;
				s += "/";
				strings.PushBack( s );
			}
			else if ( entry->d_type == DT_REG )
			{
				String s( dirPath );
				s += entry->d_name;
				strings.PushBack( s );
			}
		}
		closedir( dir );
	}
#endif

	SortStringArray( strings );

	return strings;
}

bool HasPermission( const char * fileOrDirName, const permissionFlags_t flags )
{
	OVR_ASSERT( flags.GetValue() != 0 );

	String s( fileOrDirName );
	int len = static_cast<int>( s.GetSize() );
	if ( s[ len - 1 ] != '/' )
	{	// directory ends in a slash
		int	end = len - 1;
		for ( ; end > 0 && s[ end ] != '/'; end-- )
			;
		s = String( &s[ 0 ], end );
	}

	int mode = 0;
	if ( flags & PERMISSION_WRITE )
	{
		mode |= W_OK;
	}
	if ( flags & PERMISSION_READ )
	{
		mode |= R_OK;
	}
#if defined( OVR_OS_ANDROID )
	if ( flags & PERMISSION_EXECUTE )
	{
		mode |= X_OK;
	}
	return access( s.ToCStr(), mode ) == 0;
#else
	return _access( s.ToCStr(), mode ) == 0;
#endif
}

bool FileExists( const char * filename )
{
	struct stat st;
	int result = stat( filename, &st );
	return result == 0;
}

bool IsFolder( const char * folderName )
{
	struct stat st;
	int result = stat( folderName, &st );
	if ( result != 0 )
		return false;
#if defined( OVR_OS_ANDROID )
	if ( S_ISDIR( st.st_mode ) )
	{
		return true;
	}
#endif
	return false;
}

bool MatchesExtension( const char * fileName, const char * ext )
{
	const int extLen = static_cast<int>( OVR_strlen( ext ) );
	const int sLen = static_cast<int>( OVR_strlen( fileName ) );
	if ( sLen < extLen + 1 )
	{
		return false;
	}
	return ( 0 == strcmp( &fileName[ sLen - extLen ], ext ) );
}

String ExtractFileBase( const String & s )
{
	const int l = static_cast<int>( s.GetSize() );
	if ( l == 0 )
	{
		return String( "" );
	}

	int	end;
	if ( s[ l - 1 ] == '/' )
	{	// directory ends in a slash
		end = l - 1;
	}
	else
	{
		for ( end = l - 1; end > 0 && s[ end ] != '.'; end-- )
			;
		if ( end == 0 )
		{
			end = l;
		}
	}
	int	start;
	for ( start = end - 1; start > -1 && s[ start ] != '/'; start-- )
		;
	start++;

	return String( &s[ start ], end - start );
}

String ExtractFile( const String & s )
{
	const int l = static_cast<int>( s.GetSize() );
	if ( l == 0 )
	{
		return String( "" );
	}

	int	end = l;
	if ( s[ l - 1 ] == '/' )
	{	// directory ends in a slash
		end = l - 1;
	}

	int	start;
	for ( start = end - 1; start > -1 && s[ start ] != '/'; start-- )
		;
	start++;

	return String( &s[ start ], end - start );
}

String ExtractDirectory( const String & s )
{
	const int l = static_cast<int>( s.GetSize() );
	if ( l == 0 )
	{
		return String( "" );
	}

	int	end;
	if ( s[ l - 1 ] == '/' )
	{	// directory ends in a slash
		end = l - 1;
	}
	else
	{
		for ( end = l - 1; end > 0 && s[ end ] != '/'; end-- )
			;
		if ( end == 0 )
		{
			end = l - 1;
		}
	}
	int	start;
	for ( start = end - 1; start > -1 && s[ start ] != '/'; start-- )
		;
	start++;

	return String( &s[ start ], end - start );
}

void MakePath( const char * dirPath, permissionFlags_t permissions )
{
	char path[ 256 ];
	char * currentChar = NULL;

	mode_t mode = 0;
	if ( permissions & PERMISSION_READ )
	{
		mode |= S_IRUSR;
	}

	if ( permissions & PERMISSION_WRITE )
	{
		mode |= S_IWUSR;
	}

	OVR_sprintf( path, sizeof( path ), "%s", dirPath );
	
	for ( currentChar = path + 1; *currentChar; ++currentChar )
	{
		if ( *currentChar == '/' ) 
		{
			*currentChar = 0;
#if defined( OVR_OS_ANDROID )
			DIR * checkDir = opendir( path );
			if ( checkDir == NULL )
			{
				mkdir( path, mode );
			}
			else
			{
				closedir( checkDir );
			}
#elif defined( OVR_OS_WIN32 )
			CreateDirectory( path, NULL );			
#endif
			*currentChar = '/';
		}
	}
}

// Returns true if head equals check plus zero or more characters.
bool MatchesHead( const char * head, const char * check )
{
	const int l = static_cast<int>( OVR_strlen( head ) );
	return 0 == OVR_strncmp( head, check, l );
}

float LinearRangeMapFloat( float inValue, float inStart, float inEnd, float outStart, float outEnd )
{
	float outValue = inValue;
	if ( fabsf( inEnd - inStart ) < MATH_FLOAT_SMALLEST_NON_DENORMAL )
	{
		return 0.5f * ( outStart + outEnd );
	}
	outValue -= inStart;
	outValue /= ( inEnd - inStart );
	outValue *= ( outEnd - outStart );
	outValue += outStart;
	if ( fabsf( outValue ) < MATH_FLOAT_SMALLEST_NON_DENORMAL )
	{
		return 0.0f;
	}
	return outValue;
}

}	// namespace OVR

