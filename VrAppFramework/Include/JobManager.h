/************************************************************************************

Filename    :   JobManager.h
Content     :   A multi-threaded job manager
Created     :   12/15/2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_JobManager_h )
#define OVR_JobManager_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_Threads.h"

namespace OVR
{

class ovrJobManager;
class ovrJobThread;

//==============================================================
// ovrJobThreadContext
class ovrJobThreadContext 
{
public:
	ovrJobThreadContext( JavaVM * javaVm, JNIEnv * jni )
		: Jvm( javaVm )
		, Jni( jni )
	{
	}

	JavaVM *	GetJvm() const { return Jvm; }
	JNIEnv *	GetJni() const { return Jni; }

private:
	ovrJobThreadContext( ovrJobThreadContext const & others ) = delete;
	ovrJobThreadContext operator = ( ovrJobThreadContext const & rhs ) = delete;

	JavaVM *			Jvm;
	JNIEnv *			Jni;
};

//==============================================================
// ovrJob
class ovrJob
{
public:
	friend class ovrJobThread;

	ovrJob( char const * name );
	virtual ~ovrJob() { }

	threadReturn_t			DoWork( ovrJobThreadContext const & jtc );

	char const *			GetName() const { return &Name[0]; }

	virtual	uint32_t		GetTypeId() const = 0;

private:
	virtual threadReturn_t	DoWork_Impl( ovrJobThreadContext const & jtc ) = 0;

private:
	char					Name[128];
};

//==============================================================
// ovrJobT
// This is just a helpful wrapper that allows you to set a type id
// as a template parm. To use it, derive your custom job class from
// this class like so:
// class ovrMyJob : public ovrJobT< MY_JOB_TYPE_ID >
// where MY_JOB_TYPE_ID is some compile-time constant.
template< uint32_t TypeId >
class ovrJobT : public ovrJob
{
public:
	ovrJobT( char const * name ) 
		: ovrJob( name )
	{
	}

	virtual	uint32_t	GetTypeId() const OVR_OVERRIDE { return TypeId; }
};

//==============================================================
// ovrJobResult
class ovrJobResult
{
public:
	ovrJobResult()
		: Job( nullptr )
		, Succeeded( false )
	{
	}

	ovrJobResult( ovrJob * job, bool const succeeded )
		: Job( job )
		, Succeeded( succeeded )
	{
	}

	ovrJob *	Job;
	bool		Succeeded;
};

//==============================================================
// ovrJobManager
class ovrJobManager
{
public:
	virtual	~ovrJobManager() { }

	static ovrJobManager *	Create( JavaVM & javaVm );
	static void				Destroy( ovrJobManager * & jm );

	virtual void	Init( JavaVM & javaVM ) = 0;
	virtual void	Shutdown() = 0;

	virtual void	EnqueueJob( ovrJob * job ) = 0;

	virtual void	ServiceJobs( OVR::Array< ovrJobResult > & finishedJobs ) = 0;

	virtual bool 	IsExiting() const = 0;
};

}	// namespace OVR

#endif // OVR_JobManager_h
