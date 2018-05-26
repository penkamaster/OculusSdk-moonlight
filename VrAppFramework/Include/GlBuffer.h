/************************************************************************************

Filename    :   GlBuffer.h
Content     :   OpenGL Buffer Management.
Created     :   
Authors     :   

Copyright   :   Copyright 2016 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_GlBuffer_h
#define OVR_GlBuffer_h

#include "Kernel/OVR_Types.h"

namespace OVR {

enum GlBufferType_t
{
	GLBUFFER_TYPE_UNIFORM,
};

class GlBuffer
{
public:
					GlBuffer();

	bool			Create( const GlBufferType_t type, const size_t dataSize, const void * data );
	void			Destroy();

	void			Update( const size_t updateDataSize, const void * data ) const;

	void *			MapBuffer() const;
	void			UnmapBuffer() const;

	unsigned int	GetBuffer() const { return buffer; }

private:
	unsigned int	target;
	unsigned int	buffer;
	size_t			size;
};

}	// namespace OVR

#endif	// !OVR_GlBuffer_h
