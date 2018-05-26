/************************************************************************************

Filename    :   ModelFileLoading.h
Content     :   Model file loading.
Created     :   December 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef MODELFILELOADING_H
#define MODELFILELOADING_H

#include "ModelFile.h"

#include <math.h>

#include "Kernel/OVR_Alg.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String_Utils.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_BinaryFile.h"

#include "OVR_LogTimer.h"			// for LOGCPUTIME

#include "unzip.h"

// Verbose log, redefine this as LOG() to get lots more info dumped
//#define LOGV LOG
#define LOGV(...)

namespace OVR {

void CalculateTransformFromRTS( Matrix4f * localTransform, const Quatf rotation, const Vector3f translation, const Vector3f scale );

void LoadModelFileTexture( ModelFile & model, const char * textureName,
	const char * buffer, const int size, const MaterialParms & materialParms );

bool LoadModelFile_OvrScene( ModelFile * modelPtr, unzFile zfp, const char * fileName,
	const char * fileData, const int fileDataLength,
	const ModelGlPrograms & programs,
	const MaterialParms & materialParms,
	ModelGeo * outModelGeo = NULL );

bool LoadModelFile_glTF_OvrScene( ModelFile * modelFilePtr, unzFile zfp, const char * fileName,
	const char * fileData, const int fileDataLength,
	const ModelGlPrograms & programs,
	const MaterialParms & materialParms,
	ModelGeo * outModelGeo = NULL );

ModelFile * LoadModelFile_glB( const char * fileName, 
	const char * fileData, const int fileDataLength,
	const ModelGlPrograms & programs,
	const MaterialParms & materialParms,
	ModelGeo * outModelGeo = NULL );

} // namespace OVR

#endif	// MODELFILELOADING_H
