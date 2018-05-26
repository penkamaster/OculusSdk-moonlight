/************************************************************************************

Filename    :   FileLoader.cpp
Content     :   
Created     :   August 13, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Threads.h"
#include "PackageFiles.h"
#include "FileLoader.h"
#include "Oculus360Photos.h"
#include "turbojpeg.h"
#include "OVR_TurboJpeg.h"

namespace OVR {

ovrMessageQueue		Queue1( 4000 );	// big enough for all the thumbnails that might be needed
ovrMessageQueue		Queue3( 1 );

Mutex 			QueueMutex;
WaitCondition	QueueWake;
bool			QueueHasCleared = true;

void * Queue1Thread( Thread * thread, void * v )
{
	OVR_UNUSED( v );

	thread->SetThreadName( "FileQueue1" );

	// Process incoming messages until queue is empty
	for ( ; ; )
	{
		Queue1.SleepUntilMessage();

		// If Queue3 hasn't finished our last output, sleep until it has.
		QueueMutex.DoLock();
		while ( !QueueHasCleared )
		{
			// Atomically unlock the mutex and block until we get a message.
			QueueWake.Wait( &QueueMutex );
		}
		QueueMutex.Unlock();

		const char * msg = Queue1.GetNextMessage();

		char commandName[1024] = {};
		sscanf( msg, "%s", commandName );
		const char * filename = msg + strlen( commandName ) + 1;

		ovrMessageQueue * queue = &Queue3;
		char const * suffix = strstr( filename, "_nz.jpg" ); 
		if ( suffix != NULL )	
		{
			// cube map
			const char * const cubeSuffix[6] = { "_px.jpg", "_nx.jpg", "_py.jpg", "_ny.jpg", "_pz.jpg", "_nz.jpg" };
			
			MemBufferFile mbfs[6] = 
			{ 
				MemBufferFile( MemBufferFile::NoInit ), 
				MemBufferFile( MemBufferFile::NoInit ),
				MemBufferFile( MemBufferFile::NoInit ),
				MemBufferFile( MemBufferFile::NoInit ),
				MemBufferFile( MemBufferFile::NoInit ),
				MemBufferFile( MemBufferFile::NoInit ) 
			};

			char filenameWithoutSuffix[1024];
			int suffixStart = int( suffix - filename );
			strcpy( filenameWithoutSuffix, filename );
			filenameWithoutSuffix[suffixStart] = '\0';
			
			int side = 0;
			for ( ; side < 6; side++ )
			{
				char sideFilename[1024];
				strcpy( sideFilename, filenameWithoutSuffix );
				strcat( sideFilename, cubeSuffix[side] );
				if ( !mbfs[side].LoadFile( sideFilename ) )
				{
					if ( !ovr_ReadFileFromApplicationPackage( sideFilename, mbfs[ side ] ) )
					{
						break;
					}
				}
				LOG( "Queue1 loaded '%s'", sideFilename );
			}
			if ( side >= 6 )
			{
				// if no error occured, post to next thread
				LOG( "%s.PostPrintf( \"%s %p %i %p %i %p %i %p %i %p %i %p %i\" )", "Queue3", commandName, 
						mbfs[0].Buffer, mbfs[0].Length,
						mbfs[1].Buffer, mbfs[1].Length,
						mbfs[2].Buffer, mbfs[2].Length,
						mbfs[3].Buffer, mbfs[3].Length,
						mbfs[4].Buffer, mbfs[4].Length,
						mbfs[5].Buffer, mbfs[5].Length );
				queue->PostPrintf( "%s %p %i %p %i %p %i %p %i %p %i %p %i", commandName,
						mbfs[0].Buffer, mbfs[0].Length,
						mbfs[1].Buffer, mbfs[1].Length,
						mbfs[2].Buffer, mbfs[2].Length,
						mbfs[3].Buffer, mbfs[3].Length,
						mbfs[4].Buffer, mbfs[4].Length,
						mbfs[5].Buffer, mbfs[5].Length );
				for ( int i = 0; i < 6; ++i )
				{
					// make sure we do not free the actual buffers because they're used in the next thread
					mbfs[i].Buffer = NULL;
					mbfs[i].Length = 0;
				}
			}
			else
			{
				// otherwise free the buffers we did manage to allocate
				for ( int i = 0; i < side; ++i )
				{
					mbfs[i].FreeData();
				}
			}
		}
		else
		{
			// non-cube map
			MemBufferFile mbf( filename );
			if ( mbf.Length <= 0 || mbf.Buffer == NULL )
			{
				if ( !ovr_ReadFileFromApplicationPackage( filename, mbf ) )
				{
					continue;
				}
			}
			LOG( "%s.PostPrintf( \"%s %p %i\" )", "Queue3", commandName, mbf.Buffer, mbf.Length );
			queue->PostPrintf( "%s %p %i", commandName, mbf.Buffer, mbf.Length );
			mbf.Buffer = NULL;
			mbf.Length = 0;
		}

		free( (void *)msg );
	}

#if defined( OVR_OS_ANDROID )
	LOG( "FileLoadThread returned" );
	return NULL;
#endif
}

void * Queue3Thread( Thread * thread, void * v )
{
	thread->SetThreadName( "FileQueue3" );

	// Process incoming messages until queue is empty
	for ( ; ; )
	{
		Queue3.SleepUntilMessage();
		const char * msg = Queue3.GetNextMessage();

		LOG( "Queue3 msg = '%s'", msg );

		// Note that Queue3 has cleared the message
		QueueMutex.DoLock();
		QueueHasCleared = true;
		QueueWake.NotifyAll();
		QueueMutex.Unlock();

		char commandName[1024] = {};
		sscanf( msg, "%s", commandName );
		int numBuffers = strcmp( commandName, "cube" ) == 0 ? 6 : 1;
		unsigned * b[6] = {};
		int blen[6];
		if ( numBuffers == 1 )
		{
			sscanf( msg, "%s %p %i", commandName, &b[0], &blen[0] );
		}
		else
		{
			OVR_ASSERT( numBuffers == 6 );		
			sscanf( msg, "%s %p %i %p %i %p %i %p %i %p %i %p %i ", commandName,
					&b[0], &blen[0],
					&b[1], &blen[1],
					&b[2], &blen[2],
					&b[3], &blen[3],
					&b[4], &blen[4],
					&b[5], &blen[5] );
		}

#define USE_TURBO_JPEG
#if !defined( USE_TURBO_JPEG )
		stbi_uc * data[6];
#else
		unsigned char * data[6];
#endif

		int resolutionX = 0;
		int resolutionY = 0;
		int buffCount = 0;
		for ( ; buffCount < numBuffers; buffCount++ )
		{
			int	x, y;
			unsigned * b1 = b[buffCount];
			int b1len = blen[buffCount];

#if !defined( USE_TURBO_JPEG )
			int comp;
			data[buffCount] = stbi_load_from_memory( (const stbi_uc*)b1, b1len, &x, &y, &comp, 4 );
#else
			data[buffCount] = TurboJpegLoadFromMemory( (unsigned char*)b1, b1len, &x, &y );
#endif
			if ( buffCount == 0 )
			{
				resolutionX = x;
				resolutionY = y;
			}

			// done with the loading buffer now
			free( b1 );

			if ( data[buffCount] == NULL )
			{
				LOG( "LoadingThread: failed to load from buffer" );
				break;
			}
		}

		if ( buffCount != numBuffers )	// an image load failed, free everything and abort this load
		{
			for ( int i = 0; i < numBuffers; ++i )
			{
				free( data[i] );
				data[i] = NULL;
			}
		}
		else
		{
			if ( numBuffers == 1 )
			{
				OVR_ASSERT( data[0] != NULL );
				LOG( "Queue3.PostPrintf( \"%s %p %i %i\" )", commandName, data[0], resolutionX, resolutionY );
				( ( Oculus360Photos * )v )->GetBGMessageQueue( ).PostPrintf( "%s %p %i %i", commandName, data[ 0 ], resolutionX, resolutionY );
			}
			else 
			{
				OVR_ASSERT( numBuffers == 6 );
				LOG( "Queue3.PostPrintf( \"%s %i %p %p %p %p %p %p\" )", commandName, resolutionX, 
						data[0], data[1], data[2], data[3], data[4], data[5] );
				( ( Oculus360Photos * )v )->GetBGMessageQueue( ).PostPrintf( "%s %i %p %p %p %p %p %p", commandName, resolutionX,
						data[0], data[1], data[2], data[3], data[4], data[5] );
			}
		}

		free( (void *)msg );
	}

#if defined( OVR_OS_ANDROID )
	LOG( "FileLoadThread returned" );
	return NULL;
#endif
}

Thread appThread;
Thread photosThread;

void InitFileQueue( App * app, Oculus360Photos * photos )
{
    appThread = Thread( Thread::CreateParams( &Queue1Thread, app, 128 * 1024, -1, Thread::NotRunning, Thread::NormalPriority) );
	appThread.Start();

	photosThread = Thread( Thread::CreateParams( &Queue3Thread, photos, 128 * 1024, -1, Thread::NotRunning, Thread::NormalPriority) );
	photosThread.Start();
}

}	// namespace OVR
