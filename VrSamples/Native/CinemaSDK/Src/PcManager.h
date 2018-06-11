/************************************************************************************

Filename    :   PcManager.h
Content     :
Created     :	9/10/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( PcManager_h )
#define PcManager_h

#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"
#include "GlTexture.h"
#include "Native.h"


namespace OculusCinema {

class CinemaApp;

using namespace OVR;

enum MovieFormat
{
	VT_UNKNOWN,
	VT_2D,
	VT_LEFT_RIGHT_3D,			// Left & right are scaled horizontally by 50%.
	VT_LEFT_RIGHT_3D_CROP,        // Left & right are unscaled but top and bottom 25% need to be cropped
	VT_LEFT_RIGHT_3D_FULL,		// Left & right are unscaled.
	VT_TOP_BOTTOM_3D,			// Top & bottom are scaled vertically by 50%.
	VT_TOP_BOTTOM_3D_FULL,		// Top & bottom are unscaled.
};

enum PcCategory {
    CATEGORY_LIMELIGHT,
    CATEGORY_REMOTEDESKTOP,
    CATEGORY_VNC
};

class PcDef
{
public:
	    String            Name;
    String            PosterFileName;
    String            UUID;
    String            Binding;
    int                Id;
	bool            isRunning;
	bool            isRemote;

	GLuint			Poster;
	int				PosterWidth;
	int				PosterHeight;
	int 			ResWidth;
	int 			ResHeight;
	PcCategory        Category;


	    PcDef() : Name(), PosterFileName(), UUID(), Binding(), Poster( 0 ), PosterWidth( 0 ), PosterHeight( 0 ),
            Category( CATEGORY_LIMELIGHT ) {}


};

class PcManager
{
public:
							PcManager( CinemaApp &cinema );
							~PcManager();

	void					OneTimeInit( const char * launchIntent );
	void					OneTimeShutdown();

    void
    AddPc(const char *name, const char *uuid, Native::PairState pairState, Native::Reachability reachability, const char *binding, const bool isRunning);

	void                    RemovePc(const String &name);
    void                    LoadPcs();
    Array<const PcDef *>    GetPcList( PcCategory category ) const;

public:
    Array<PcDef *> 		Movies;

    static const int 		PosterWidth;
    static const int 		PosterHeight;

    bool                    updated;

private:
	CinemaApp &				Cinema;

	PcManager &			operator=( const PcManager & );

public:
    GLuint                    PcPoster;
private:
    GLuint                    PcPosterPaired;
    GLuint                    PcPosterUnpaired;
    GLuint                    PcPosterUnknown;
    GLuint                    PcPosterWTF;


    PcCategory                 CategoryFromString( const String &categoryString ) const;
    virtual void             ReadMetaData( PcDef *aPc );

};

} // namespace OculusCinema

#endif // MovieManager_h
