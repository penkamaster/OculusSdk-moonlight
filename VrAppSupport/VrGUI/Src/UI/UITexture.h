/************************************************************************************

Filename    :   UITexture.h
Content     :
Created     :	1/8/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( UITexture_h )
#define UITexture_h

#include "Kernel/OVR_MemBuffer.h"
#include "OVR_GlUtils.h"
#include "OVR_FileSys.h"

namespace OVR {

class UITexture
{
public:
										UITexture();
	virtual								~UITexture();

	void 								Free();

	void 								LoadTextureFromApplicationPackage( const char * assetPath );	// Depricated.  Use LoadTextureFromUri.
	void 								LoadTextureFromUri( ovrFileSys & fileSys, const char * uri );
	void 								LoadTextureFromBuffer( const char * fileName, const OVR::MemBuffer & buffer );
	void 								LoadTextureFromMemory( const uint8_t * data, const int width, const int height );

	void								SetTexture( const GLuint texture, const int width, const int height, const bool freeTextureOnDestruct );


	int 								Width;
	int									Height;
	GLuint 								Texture;
	bool								FreeTextureOfDestruct;
};

} // namespace OVR

#endif // UITexture_h
