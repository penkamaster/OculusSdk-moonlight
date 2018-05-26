/************************************************************************************

Filename    :   OVR_TurboJpeg.h
Content     :	Utility functions for libjpeg-turbo
Created     :   February 25, 2015
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#ifndef OVR_TJUTIL_H_
#define	OVR_TJUTIL_H_

namespace OVR {

bool WriteJpeg( const char * destinationFile, const unsigned char * rgbxBuffer, int width, int height );

// Drop-in replacement for stbi_load_from_memory(), but without component specification.
// Often 2x - 3x faster.
unsigned char * TurboJpegLoadFromMemory( const unsigned char * jpg, const int length, int * width, int * height );

unsigned char * TurboJpegLoadFromFile( const char * filename, int * width, int * height );

}
#endif // OVR_TJUTIL_H_
