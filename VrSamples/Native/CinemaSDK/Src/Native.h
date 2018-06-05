/************************************************************************************

Filename    :   Native.h
Content     :
Created     :	8/8/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( Native_h )
#define Native_h
#include "App.h"

using namespace OVR;

namespace OculusCinema {

class Native {
public:
	static void			OneTimeInit( App *app, jclass mainActivityClass );
	static void			OneTimeShutdown();

	static String		GetExternalCacheDirectory( App *app );  	// returns path to app specific writable directory
	static bool         CreateVideoThumbnail( App *app, const char *uuid, int appId, const char *outputFilePath, const int width, const int height );

	static bool			IsPlaying( App *app );
	static bool 		IsPlaybackFinished( App *app );
	static bool 		HadPlaybackError( App *app );

	static void         StartMovie( App *app, const char * uuid, const char * appName, int id, const char * binder, int width, int height, int fps, bool hostAudio );


	static void 		StopMovie( App *app );
	
    enum PairState {
        NOT_PAIRED = 0,
        PAIRED,
        PIN_WRONG,
        FAILED
    };

    enum CompState {
        ONLINE = 0,
        OFFLINE,
        UNKNOWN_STATE
    };

    enum Reachability {
        LOCAL = 0,
        REMOTE,
        RS_OFFLINE,
        UNKNOWN_REACH
    };

    static void            InitPcSelector( App *app );
    static void            InitAppSelector( App *app, const char* uuid);
    static PairState       GetPairState( App *app, const char* uuid);
    static void            Pair( App *app, const char* uuid);
	static void            MouseMove(App *app, int deltaX, int deltaY);
	static void            MouseClick(App *app, int buttonId, bool down);
	static void            MouseScroll(App *app, signed char amount);
	static int             addPCbyIP(App *app, const char* ip);



};

} // namespace OculusCinema

#endif // Native_h
