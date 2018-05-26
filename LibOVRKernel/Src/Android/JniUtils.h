/************************************************************************************

Filename    :   JniUtils.h
Content     :   JNI utility functions
Created     :   October 21, 2014
Authors     :   J.M.P. van Waveren, Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_JniUtils_h
#define OVR_JniUtils_h

#include "Kernel/OVR_LogUtils.h"

//==============================================================
// Various helper functions.
//==============================================================

enum
{
	ANDROID_KITKAT			= 19,		// Build.VERSION_CODES.KITKAT
	ANDROID_KITKAT_WATCH	= 20,		// Build.VERSION_CODES.KITKAT_WATCH
	ANDROID_LOLLIPOP		= 21,		// Build.VERSION_CODES.LOLLIPOP
	ANDROID_LOLLIPOP_MR1	= 22,		// Build.VERSION_CODES.LOLLIPOP_MR1
	ANDROID_MARSHMALLOW		= 23,		// Build.VERSION_CODES.M
	ANDROID_NOUGAT			= 24,		// Build.VERSION_CODES.N
	ANDROID_N_MR1			= 25,		// Build.VERSION_CODES.N_MR1
};

int				ovr_GetBuildVersionSDK( JNIEnv * jni );

// JVM's AttachCurrentThread() resets the thread name if the thread name is not passed as an argument.
// Use these wrappers to attach the current thread to a JVM without resetting the current thread name.
// These will fatal error when unsuccesful.
jint			ovr_AttachCurrentThread( JavaVM * vm, JNIEnv ** jni, void * args );
jint			ovr_DetachCurrentThread( JavaVM * vm );

// Every Context or its subclass (ex: Activity) has a ClassLoader
jclass			ovr_GetClassLoader( JNIEnv * jni, jobject contextObject );

// FindClass uses the current callstack to determine which ClassLoader object to use
// to start the class search. As a result, an attempt to find an app-specific class will
// fail when FindClass is called from an arbitrary thread with the JavaVM started in
// the "system" class loader, instead of the one associated with the application.
// The following two functions can be used to find any class from any thread. These will
// fatal error if the class is not found.
jclass			ovr_GetLocalClassReferenceWithLoader( JNIEnv * jni, jobject classLoader, const char * className );
jclass			ovr_GetGlobalClassReferenceWithLoader( JNIEnv * jni, jobject classLoader, const char * className );

// The following two functions can be used to find any class from any thread, but they do
// need the activity object to explicitly start the class search using the class loader
// associated with the application. These will fatal error if the class is not found.
jclass			ovr_GetLocalClassReference( JNIEnv * jni, jobject activityObject, const char * className );
jclass			ovr_GetGlobalClassReference( JNIEnv * jni, jobject activityObject, const char * className );

// These will fatal error if the method is not found.
jmethodID		ovr_GetMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature );
jmethodID		ovr_GetStaticMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature );

// Use this EVERYWHERE and you can insert your own catch here if you have string references leaking.
// Even better, use the JavaString / JavaUTFChars classes instead and they will free resources for
// you automatically.
jobject			ovr_NewStringUTF( JNIEnv * jni, char const * str );
char const *	ovr_GetStringUTFChars( JNIEnv * jni, jstring javaStr, jboolean * isCopy );

// Get the code path of the current package.
const char *	ovr_GetPackageCodePath( JNIEnv * jni, jobject activityObject, char * packageCodePath, int const maxLen );

// If the specified package is found, returns true and the full path for the specified package is copied
// into packagePath. If the package was not found, false is returned and packagePath will be an empty string.
bool			ovr_GetInstalledPackagePath( JNIEnv * jni, jobject activityObject, char const * packageName, 
						char * packagePath, int const packagePathSize );

// Get the current package name, for instance "com.oculus.systemactivities".
const char *	ovr_GetCurrentPackageName( JNIEnv * jni, jobject activityObject, char * packageName, int const maxLen );

// Get the current activity name, for instance "com.oculus.systemactivities.PlatformActivity".
const char *	ovr_GetCurrentActivityName( JNIEnv * jni, jobject activityObject, char * activityName, int const maxLen );

// For instance packageName = "com.oculus.systemactivities".
bool			ovr_IsCurrentPackage( JNIEnv * jni, jobject activityObject, const char * packageName );

// For instance activityName = "com.oculus.systemactivities.PlatformActivity".
bool			ovr_IsCurrentActivity( JNIEnv * jni, jobject activityObject, const char * activityName );

#if defined( OVR_OS_ANDROID )
//==============================================================
// TempJniEnv
//
// Gets a temporary JNIEnv for the current thread and releases the
// JNIEnv on destruction if the calling thread was not already attached.
//==============================================================
class TempJniEnv
{
public:
	TempJniEnv( JavaVM * vm_ ) :
		Vm( vm_ ),
		Jni( NULL ),
		PrivateEnv( false )
	{
		if ( JNI_OK != Vm->GetEnv( reinterpret_cast<void**>( &Jni ), JNI_VERSION_1_6 ) )
		{
			LOG( "Creating temporary JNIEnv. This is a heavy operation and should be infrequent. To optimize, use JNI AttachCurrentThread on calling thread" );
			ovr_AttachCurrentThread( Vm, &Jni, NULL );
			PrivateEnv = true;
		}
		else
		{
			// Current thread is attached to the VM.
			//LOG( "Using caller's JNIEnv" );
		}
	}
	~TempJniEnv()
	{
		if ( PrivateEnv )
		{
			ovr_DetachCurrentThread( Vm );
		}
	}

	operator JNIEnv * () { return Jni; }
	JNIEnv * operator-> () { return Jni; }

private:
	JavaVM *	Vm;
	JNIEnv *	Jni;
	bool		PrivateEnv;
};
#endif

//==============================================================
// JavaObject
//
// Releases a java object local reference on destruction.
//==============================================================
class JavaObject
{
public:
	JavaObject( JNIEnv * jni_, jobject const object_ ) :
		Jni( jni_ ),
		Object( object_ )
	{
		OVR_ASSERT( Jni != NULL );
	}
	~JavaObject()
	{
#if defined( OVR_OS_ANDROID )
		if ( Jni->ExceptionOccurred() )
		{
			LOG( "JNI exception before DeleteLocalRef!" );
			Jni->ExceptionClear();
		}
		OVR_ASSERT( Jni != NULL && Object != NULL );
		Jni->DeleteLocalRef( Object );
		if ( Jni->ExceptionOccurred() )
		{
			LOG( "JNI exception occurred calling DeleteLocalRef!" );
			Jni->ExceptionClear();
		}
#endif
		Jni = NULL;
		Object = NULL;
	}

	jobject			GetJObject() const { return Object; }

	JNIEnv *		GetJNI() const { return Jni; }

protected:
	void			SetJObject( jobject const & obj ) { Object = obj; }

private:
	JNIEnv *		Jni;
	jobject			Object;
};

//==============================================================
// JavaClass
//
// Releases a java class local reference on destruction.
//==============================================================
class JavaClass : public JavaObject
{
public:
	JavaClass( JNIEnv * jni_, jobject const class_ ) :
		JavaObject( jni_, class_ )
	{
	}

	jclass			GetJClass() const { return static_cast< jclass >( GetJObject() ); }
};

#if defined( OVR_OS_ANDROID )
//==============================================================
// JavaFloatArray
//
// Releases a java float array local reference on destruction.
//==============================================================
class JavaFloatArray : public JavaObject
{
public:
	JavaFloatArray( JNIEnv * jni_, jfloatArray const floatArray_ ) :
		JavaObject( jni_, floatArray_ )
	{
	}

	jfloatArray		GetJFloatArray() const { return static_cast< jfloatArray >( GetJObject() ); }
};

//==============================================================
// JavaObjectArray
//
// Releases a java float array local reference on destruction.
//==============================================================
class JavaObjectArray : public JavaObject
{
public:
	JavaObjectArray( JNIEnv * jni_, jobjectArray const floatArray_ ) :
		JavaObject( jni_, floatArray_ )
	{
	}

	jobjectArray		GetJobjectArray() const { return static_cast< jobjectArray >( GetJObject() ); }
};
#endif

//==============================================================
// JavaString
//
// Creates a java string on construction and releases it on destruction.
//==============================================================
class JavaString : public JavaObject
{
public:
	JavaString( JNIEnv * jni_, char const * string_ ) :
		JavaObject( jni_, NULL )
	{
#if defined( OVR_OS_ANDROID )
		SetJObject( GetJNI()->NewStringUTF( string_ ) );
		if ( GetJNI()->ExceptionOccurred() )
		{
			LOG( "JNI exception occurred calling NewStringUTF!" );
		}
#else
		OVR_UNUSED( string_ );
#endif
	}

	JavaString( JNIEnv * jni_, jstring string_ ) :
		JavaObject( jni_, string_ )
	{
		OVR_ASSERT( string_ != NULL );
	}

	jstring			GetJString() const { return static_cast< jstring >( GetJObject() ); }
};

//==============================================================
// JavaUTFChars
//
// Gets a java string object as a buffer of UTF characters and
// releases the buffer on destruction.
// Use this only if you need to store a string reference and access
// the string as a char buffer of UTF8 chars.  If you only need
// to store and release a reference to a string, use JavaString.
//==============================================================
class JavaUTFChars : public JavaString
{
public:
	JavaUTFChars( JNIEnv * jni_, jstring const string_ ) :
		JavaString( jni_, string_ ),
		UTFString( NULL )
	{
#if defined( OVR_OS_ANDROID )
		UTFString = GetJNI()->GetStringUTFChars( GetJString(), NULL );
		if ( GetJNI()->ExceptionOccurred() )
		{
			LOG( "JNI exception occurred calling GetStringUTFChars!" );
		}
#endif
	}

	~JavaUTFChars()
	{
#if defined( OVR_OS_ANDROID )
		OVR_ASSERT( UTFString != NULL );
		GetJNI()->ReleaseStringUTFChars( GetJString(), UTFString );
		if ( GetJNI()->ExceptionOccurred() )
		{
			LOG( "JNI exception occurred calling ReleaseStringUTFChars!" );
		}
#endif
	}

	char const * ToStr() const { return UTFString; }
	operator char const * () const { return UTFString; }

private:
	char const *	UTFString;
};

#endif	// OVR_JniUtils_h
