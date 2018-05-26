/************************************************************************************

Filename    :   JobManager.h
Content     :   A multi-threaded job manager
Created     :   12/15/2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "JobManager.h"

#include "Android/JniUtils.h"
#include "Kernel/OVR_LogUtils.h"
#include "Kernel/OVR_Signal.h"
#include <ctime>
#include "ScopedMutex.h"

namespace OVR {

//==============================================================================================
// ovrMPMCArray
//==============================================================================================

//==============================================================
// ovrMPMCArray
//
// Multiple-producer, multiple-consumer array.
// Currently implemented with a mutex.

template< typename T >
class ovrMPMCArray
{
public:
	ovrMPMCArray() { }
	~ovrMPMCArray() { }

	T 			Pop();
	void		PushBack( T const & value );
	void		RemoveAt( size_t const index );
	void		RemoveAtUnordered( size_t const index );
	T const 	operator[] ( int const index ) const;
	T 			operator[] ( int const index );
	void		Clear();
	void		MoveArray( OVR::Array< T > & a );

private:
	OVR::Array< T >	A;
	OVR::Mutex		ThisMutex;
};

template< typename T >
T ovrMPMCArray< T >::Pop()
{
	ovrScopedMutex mutex( ThisMutex );
	if ( A.GetSizeI() <= 0 )
	{
		return nullptr;
	}
	return A.Pop();
}

template< typename T >
void ovrMPMCArray< T >::PushBack( T const & value )
{
	ovrScopedMutex mutex( ThisMutex );
	A.PushBack( value );
}

template< typename T >
void ovrMPMCArray< T >::RemoveAt( size_t const index )
{
	ovrScopedMutex mutex( ThisMutex );
	return A.RemoveAt( index );
}

template< typename T >
void ovrMPMCArray< T >::RemoveAtUnordered( size_t const index )
{
	ovrScopedMutex mutex( ThisMutex );
	return A.RemoveAtUnordered( index );
}
	
template< typename T >
T const ovrMPMCArray< T >::operator[] ( int const index ) const
{
	ovrScopedMutex mutex( ThisMutex );
	return A[index];
}

template< typename T >
T ovrMPMCArray< T >::operator[] ( int const index )
{
	ovrScopedMutex mutex( ThisMutex );
	return A[index];
}

template< typename T >
void ovrMPMCArray< T >::Clear()
{
	ovrScopedMutex mutex( ThisMutex );
	A.Clear();
}

template< typename T >
void ovrMPMCArray< T >::MoveArray( OVR::Array< T > & a )
{
	ovrScopedMutex mutex( ThisMutex );
	for ( int i = 0; i < A.GetSizeI(); ++i )
	{
		a.PushBack( A[i] );
	}
	A.Clear();
}


class ovrJobManagerImpl;

//==============================================================
// ovrJobThread
class ovrJobThread
{
public:
	static threadReturn_t Fn( Thread * thread, void * data );

	ovrJobThread( ovrJobManagerImpl * jobManager, char const * threadName )
		: JobManager( jobManager )
		, MyThread( nullptr )
		, Jni( nullptr )
		, Attached( false )
	{
		OVR_strcpy( ThreadName, sizeof( ThreadName ), threadName );
	}
	~ovrJobThread()
	{
		// verify shutown before deconstruction
		OVR_ASSERT( MyThread == nullptr );
		OVR_ASSERT( Jni == nullptr );
	}

	static ovrJobThread *	Create( ovrJobManagerImpl * jobManager, int const threadNum, const char * threadName );
	static void				Destroy( ovrJobThread * & jobThread );

	void	Init( int const threadNum );
	void	Shutdown();

	ovrJobManagerImpl *	GetJobManager() { return JobManager; }
	JNIEnv *			GetJni() { return Jni; }
	char const *		GetThreadName() const { return ThreadName; }
	bool				IsAttached() const { return Attached; }

private:
	ovrJobManagerImpl *	JobManager;	// manager that owns us
	Thread *			MyThread;	// our thread context
	JNIEnv *			Jni;		// Java environment for this thread
	char				ThreadName[16];
	bool				Attached;

private:
	void	AttachToCurrentThread();
	void	DetachFromCurrentThread();
};

//==============================================================
// ovrJobManagerImpl
class ovrJobManagerImpl : public ovrJobManager
{
public:
	friend class ovrJobThread;

	static const int	MAX_THREADS = 5;

	ovrJobManagerImpl();
	virtual ~ovrJobManagerImpl();

	void	Init( JavaVM & javaVM ) OVR_OVERRIDE;
	void	Shutdown() OVR_OVERRIDE;

	void	EnqueueJob( ovrJob * job ) OVR_OVERRIDE;

	void	ServiceJobs( OVR::Array< ovrJobResult > & finishedJobs ) OVR_OVERRIDE;

	bool	IsExiting() const OVR_OVERRIDE { return Exiting; }

	JavaVM *GetJvm() { return Jvm; }

private:
	//--------------------------
	// thread function interface
	//--------------------------
	void		JobCompleted( ovrJob * job, bool const succeeded );
	ovrJob *	GetPendingJob();
	ovrSignal *	GetNewJobSignal() { return NewJobSignal; }

private:
	OVR::Array< ovrJobThread * >	Threads;

	ovrMPMCArray< ovrJob* >			PendingJobs;	// jobs that haven't executed yet
	ovrMPMCArray< ovrJob* >			RunningJobs;	// jobs that are currently executing

	ovrMPMCArray< ovrJobResult >	CompletedJobs;	// jobs that have completed

	ovrSignal *						NewJobSignal;

	bool							Initialized;
	volatile bool					Exiting;

	JavaVM *						Jvm;

private:
	bool	StartJob( OVR::Thread * thread, ovrJob * job );
	bool	StartJob( ovrJob * job );
	void	AttachToCurrentThread();
	void	DetachFromCurrentThread();
};

//==============================================================================================
// ovrJob
//==============================================================================================
ovrJob::ovrJob( char const * name )
{
	OVR_strcpy( Name, sizeof( Name ), name );
}

threadReturn_t ovrJob::DoWork( ovrJobThreadContext const & jtc )
{
	clock_t startTime = std::clock();

	threadReturn_t tr = DoWork_Impl( jtc );

	clock_t endTime = std::clock();
	LOG( "Job '%s' took %f seconds.", Name, double( endTime - startTime ) / (double)CLOCKS_PER_SEC );

	return tr;
}

//==============================================================================================
// ovrJobThread
//==============================================================================================

threadReturn_t ovrJobThread::Fn( Thread * thread, void * data )
{
	//ovrJobManagerImpl * jm = reinterpret_cast< ovrJobManagerImpl* >( data );
	ovrJobThread * jt = reinterpret_cast< ovrJobThread* >( data );
	ovrJobManagerImpl * jm = jt->GetJobManager();

	thread->SetThreadName( jt->GetThreadName() );

	jt->AttachToCurrentThread();

	while ( !jm->IsExiting() )
	{
		ovrJob * job = jm->GetPendingJob();
		if ( job != nullptr )
		{
			ovrJobThreadContext context( jm->GetJvm(), jt->GetJni() );
			threadReturn_t r = job->DoWork( context );
			jm->JobCompleted( job, r != nullptr );
		}
		else
		{
			jm->GetNewJobSignal()->Wait( -1 );
		}
	}

	jt->DetachFromCurrentThread();

	return (void*)0;
}

ovrJobThread * ovrJobThread::Create( ovrJobManagerImpl * jobManager, int const threadNum, 
		char const * threadName )
{
	ovrJobThread * jobThread = new ovrJobThread( jobManager, threadName );
	if ( jobThread == nullptr )
	{
		return nullptr;
	}
	
	jobThread->Init( threadNum );
	return jobThread;
}

void ovrJobThread::Destroy( ovrJobThread * & jobThread )
{
	OVR_ASSERT( jobThread != nullptr );
	jobThread->Shutdown();
	delete jobThread;
	jobThread = nullptr;
}

void ovrJobThread::Init( int const threadNum )
{
	OVR_ASSERT( JobManager != nullptr );
	OVR_ASSERT( MyThread == nullptr );

	size_t const stackSize = 128 * 1024;
	int const processorAffinity = -1;
	OVR::Thread::ThreadState initialState = OVR::Thread::Running;

	OVR_ASSERT( Jni == nullptr );	// this will be attached when the thread executes

	Thread::CreateParams createParams( 
			ovrJobThread::Fn, 
			this, 
			stackSize, 
			processorAffinity,
			initialState,
			Thread::IdlePriority );

	//MyThread = new OVR::Thread( ovrJobThread::Fn, this, stackSize, processorAffinity, initialState );
	MyThread = new OVR::Thread( createParams );
}

void ovrJobThread::Shutdown()
{
	OVR_ASSERT( Jni == nullptr );	// DetachFromCurrentThread should have been called first
	OVR_ASSERT( MyThread != nullptr );

	MyThread->Join();

	delete MyThread;	// this will call Join()
	MyThread = nullptr;	
}

void ovrJobThread::AttachToCurrentThread()
{
	ovr_AttachCurrentThread( JobManager->GetJvm(), &Jni, nullptr );
	Attached = true;
}

void ovrJobThread::DetachFromCurrentThread()
{
	ovr_DetachCurrentThread( JobManager->GetJvm() );
	Jni = nullptr;
	Attached = false;
}

//==============================================================================================
// ovrJobManagerImpl
//==============================================================================================
ovrJob * ovrJobManagerImpl::GetPendingJob()
{
	ovrJob * pendingJob = PendingJobs.Pop();
	return pendingJob;
}

void ovrJobManagerImpl::JobCompleted( ovrJob * job, bool const succeeded )
{
	CompletedJobs.PushBack( ovrJobResult( job, succeeded ) );
}

ovrJobManagerImpl::ovrJobManagerImpl()
	: NewJobSignal( nullptr )
	, Initialized( false )
	, Exiting( false )
	, Jvm( nullptr )
{
}

ovrJobManagerImpl::~ovrJobManagerImpl()
{
	Shutdown();
}

void ovrJobManagerImpl::Init( JavaVM & javaVM )
{
	Jvm = &javaVM;

	// signal must be created before any job threads are created
	NewJobSignal = ovrSignal::Create( true );

	// create all threads... they will end up waiting on a new job signal
	for ( int i = 0; i < MAX_THREADS; ++i )
	{
		char threadName[16];
		OVR_sprintf( threadName, sizeof( threadName ), "ovrJobThread_%i", i );

		ovrJobThread * jt = ovrJobThread::Create( this, i, threadName );
		Threads.PushBack( jt );
	}

	Initialized = true;
}

void ovrJobManagerImpl::Shutdown()
{
	LOG( "ovrJobManagerImpl::Shutdown" );

	Exiting = true;

	// allow all threads to complete their current job
	// waiting threads must timeout waiting for NewJobSignal
	while( Threads.GetSizeI() > 0 )
	{
		NewJobSignal->Raise(); // raise signal to release any waiting thread

		// loop through threads until we find one that's seen the Exiting flag and detatched
		int threadIndex = 0;
		for ( ; ; )
		{
			if ( !Threads[threadIndex]->IsAttached() )
			{
				LOG( "Exited thread '%s'", Threads[threadIndex]->GetThreadName() );
				ovrJobThread::Destroy( Threads[threadIndex] );
				Threads.RemoveAtUnordered( threadIndex );
				break;
			}
			threadIndex++;
			if ( threadIndex >= Threads.GetSizeI() )
			{
				threadIndex = 0;
			}
		}
	}

	ovrSignal::Destroy( NewJobSignal );

	Initialized = false;

	LOG( "ovrJobManagerImpl::Shutdown - complete." );
}

void ovrJobManagerImpl::EnqueueJob( ovrJob * job )
{
	//LOG( "ovrJobManagerImpl::EnqueueJob" );
	PendingJobs.PushBack( job );
	NewJobSignal->Raise();	// signal a waiting job
}

void ovrJobManagerImpl::ServiceJobs( OVR::Array< ovrJobResult > & completedJobs )
{
	CompletedJobs.MoveArray( completedJobs );
}

ovrJobManager *	ovrJobManager::Create( JavaVM & javaVm )
{
	ovrJobManager * jm = new ovrJobManagerImpl();
	if ( jm != nullptr )
	{
		jm->Init( javaVm );
	}
	return jm;
}

void ovrJobManager::Destroy( ovrJobManager * & jm )
{
	if ( jm != nullptr )
	{
		delete jm;
		jm = nullptr;
	}
}

} // namespace OVR
