/************************************************************************************

Filename    :   OVR_FileSys.h
Content     :   Abraction layer for file systems.
Created     :   July 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_FILESYS_H )
#define OVR_FILESYS_H

#include "VrApi_Types.h"
#include "Kernel/OVR_String.h"
#include "OVR_Stream.h"

namespace OVR {

//==============================================================
// ovrFileSys
class ovrFileSys
{
public:
	static const int		OVR_MAX_SCHEME_LEN		= 128;
	static const int		OVR_MAX_HOST_NAME_LEN 	= 256;
	static const int		OVR_MAX_PATH_LEN		= 1024;
	static const int		OVR_MAX_URI_LEN			= 1024;

	virtual					~ovrFileSys() { }

	// FIXME: java-specific context should eventually be abstracted
	static ovrFileSys *		Create( ovrJava const & javaContext );
	static void				Destroy( ovrFileSys * & fs );

	// Opens a stream for the specified Uri.
	virtual ovrStream *		OpenStream( char const * uri, ovrStreamMode const mode ) = 0;
	// Closes the specified stream.
	virtual void			CloseStream( ovrStream * & stream ) = 0;

	virtual bool			ReadFile( char const * uri, MemBufferT< uint8_t > & outBuffer ) = 0;

	virtual bool			FileExists( char const * uri ) = 0;
	// Gets the local path for the specified URI. File must exist. Returns false if path is not accessible directly by the file system.
	virtual bool			GetLocalPathForURI( char const * uri, String &outputPath ) = 0;
};

} // namespace OVR

#endif // OVR_FILESYS_H
