/************************************************************************************

Filename    :   Console.h
Content     :   Send commands to an application.
Created     :   11/21/2014
Authors     :   Jonathan Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrApi_Types.h"

namespace OVR {

typedef void (*consoleFn_t)( void * appPtr, const char * cmd );

void InitConsole( ovrJava & java );
void ShutdownConsole( ovrJava & java );

// add a pointer to a function that can be executed from the console
void RegisterConsoleFunction( const char * name, consoleFn_t function );
// same effect as if console recieved an intent with a command
void SendConsoleCmd( void * appPtr, const char * cmd );

void GetConsoleCmds( void( *callback )( const char * cmd ) );

extern void DebugPrint( void * appPtr, const char * cmd );

}	// namespace Ovr
