/************************************************************************************

Filename    :   PcManager.cpp
Content     :
Created     :	9/10/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "Kernel/OVR_String_Utils.h"
#include "Kernel/OVR_JSON.h"

#include "PcManager.h"
#include "CinemaApp.h"
#include "PackageFiles.h"

#if defined( OVR_OS_ANDROID )
#include <dirent.h>
#endif

#include <sys/stat.h>
#include <errno.h>

namespace OculusCinema {

const int PcManager::PosterWidth = 228;
const int PcManager::PosterHeight = 344;

//=======================================================================================

PcManager::PcManager( CinemaApp &cinema ) :
	Movies(), updated(false), Cinema(cinema)
{
}

PcManager::~PcManager()
{
}

void PcManager::OneTimeInit( const char * launchIntent )
{
	LOG( "PcManager::OneTimeInit" );

	OVR_UNUSED( launchIntent );

	const double start =  SystemClock::GetTimeInSeconds();

	    int width, height;

    PcPoster = LoadTextureFromApplicationPackage("assets/default_poster.png",
            TextureFlags_t(TEXTUREFLAG_NO_DEFAULT), width, height);
    LOG(" Default gluint: %i", PcPoster);
    PcPosterPaired = LoadTextureFromApplicationPackage(
            "assets/generic_paired_poster.png",
            TextureFlags_t(TEXTUREFLAG_NO_DEFAULT), width, height);
    PcPosterUnpaired = LoadTextureFromApplicationPackage(
            "assets/generic_unpaired_poster.png",
            TextureFlags_t(TEXTUREFLAG_NO_DEFAULT), width, height);
    PcPosterUnknown = LoadTextureFromApplicationPackage(
            "assets/generic_unknown_poster.png",
            TextureFlags_t(TEXTUREFLAG_NO_DEFAULT), width, height);
    PcPosterWTF = LoadTextureFromApplicationPackage(
            "assets/generic_wtf_poster.png",
            TextureFlags_t(TEXTUREFLAG_NO_DEFAULT), width, height);


	BuildTextureMipmaps( GlTexture( PcPosterPaired, width, height ) );
	MakeTextureTrilinear( GlTexture( PcPosterPaired, width, height ) );
	MakeTextureClamped( GlTexture( PcPosterPaired, width, height ) );


    BuildTextureMipmaps( GlTexture( PcPosterUnpaired, width, height ) );
    MakeTextureTrilinear( GlTexture( PcPosterUnpaired, width, height ) );
    MakeTextureClamped( GlTexture( PcPosterUnpaired, width, height ) );

    BuildTextureMipmaps( GlTexture( PcPosterUnknown, width, height ) );
    MakeTextureTrilinear( GlTexture( PcPosterUnknown, width, height ) );
    MakeTextureClamped( GlTexture( PcPosterUnknown, width, height ) );

    BuildTextureMipmaps( GlTexture( PcPosterWTF, width, height ) );
    MakeTextureTrilinear( GlTexture( PcPosterWTF, width, height ) );
    MakeTextureClamped( GlTexture( PcPosterWTF, width, height ) );

    LOG("PcManager::OneTimeInit: %i movies loaded, %3.1f seconds",
            Movies.GetSizeI(), vrapi_GetTimeInSeconds() - start);

}

void PcManager::OneTimeShutdown()
{
	LOG( "PcManager::OneTimeShutdown" );
}

//void PcManager::AddPc(const String &name, const String &uuid, Native::PairState pairState, Native::Reachability reachability, const String &binding, const bool isRunning) {
void PcManager::AddPc(const char *name, const char *uuid, Native::PairState pairState, Native::Reachability reachability,const char *binding, const bool isRunning) {
	PcDef *movie = NULL;
	bool isNew = false;

	for (UPInt i = 0; i < Movies.GetSize(); i++) {
		if (Movies[i]->Name.CompareNoCase(name) == 0)
			movie = Movies[i];
	}
	if (movie == NULL) {
		movie = new PcDef();
		Movies.PushBack(movie);
		isNew = true;
	}

	movie->Name = name;
	movie->UUID = uuid;
	movie->Binding = binding;
	movie->isRunning = isRunning;
	movie->isRemote = reachability == Native::REMOTE;

	//movie->ResWidth = width;
	//movie->ResHeight = height;
	if (isNew) {
		ReadMetaData(movie);
	}
	movie->Poster = PcPosterWTF;
	switch(pairState) {
	case Native::NOT_PAIRED:	movie->Poster = PcPosterUnpaired; break;
	case Native::PAIRED:		movie->Poster = PcPosterPaired; break;
	case Native::PIN_WRONG:		movie->Poster = PcPosterWTF; break;
	case Native::FAILED:
	default: 					movie->Poster = PcPosterUnknown; break;
	}
	updated = true;
}

void PcManager::RemovePc(const String &name) {
	for (UPInt i = 0; i < Movies.GetSize(); i++) {
		if (Movies[i]->Name.CompareNoCase(name) == 0)
			continue;
		Movies.RemoveAt(i);
		return;
	}
}

void PcManager::LoadPcs() {
	LOG("LoadMovies");

	const double start = vrapi_GetTimeInSeconds();

	Array<String> movieFiles; //TODO: Get enumerated PCs and updates from JNI PCSelector
	LOG("%i movies scanned, %3.1f seconds", movieFiles.GetSizeI(),
			vrapi_GetTimeInSeconds() - start);

	for (UPInt i = 0; i < movieFiles.GetSize(); i++) {
		PcDef *movie = new PcDef();
		Movies.PushBack(movie);

		movie->Name = movieFiles[i];

		ReadMetaData(movie);
	}

	LOG("%i movies panels loaded, %3.1f seconds", Movies.GetSizeI(),
			vrapi_GetTimeInSeconds() - start);
}

PcCategory PcManager::CategoryFromString(const String &categoryString) const {
	return CATEGORY_LIMELIGHT;
}

void PcManager::ReadMetaData( PcDef *movie )
{
	String filename = movie->Name;
	filename.StripExtension();
	filename.AppendString( ".txt" );

	const char* error = NULL;

	if ( !Cinema.FileExists( filename.ToCStr() ) )
	{
		return;
	}

	if (JSON* metadata = JSON::Load(filename.ToCStr(), &error)) {

		metadata->Release();

		LOG("Loaded metadata: %s", filename.ToCStr());
	} else {
		LOG("Error loading metadata for %s: %s", filename.ToCStr(),
				(error == NULL) ? "NULL" : error);
	}
}

Array<const PcDef *> PcManager::GetPcList(PcCategory category) const {
	Array<const PcDef *> result;

	for (UPInt i = 0; i < Movies.GetSize(); i++) {
		if (Movies[i]->Category == category) {
			if (Movies[i]->Poster != 0) {
				result.PushBack(Movies[i]);
			} else {
				LOG("Skipping PC with empty poster!");
			}
		}
	}

	return result;
}

} // namespace OculusCinema
