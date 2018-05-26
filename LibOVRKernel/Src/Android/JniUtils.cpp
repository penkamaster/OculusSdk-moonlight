/************************************************************************************

Filename    :   JniUtils.h
Content     :   JNI utility functions
Created     :   October 21, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "JniUtils.h"
#include "Kernel/OVR_Std.h"

#if defined( OVR_OS_ANDROID )
#include <unistd.h>
#include <pthread.h>

int ovr_GetBuildVersionSDK( JNIEnv * jni )
{
	int buildVersionSDK = 0;

	jclass versionClass = jni->FindClass( "android/os/Build$VERSION" );
	if ( versionClass != 0 )
	{
		jfieldID sdkIntFieldID = jni->GetStaticFieldID( versionClass, "SDK_INT", "I" );
		if ( sdkIntFieldID != 0 )
		{
			buildVersionSDK = jni->GetStaticIntField( versionClass, sdkIntFieldID );
		}
		jni->DeleteLocalRef( versionClass );
	}
	return buildVersionSDK;
}

jint ovr_AttachCurrentThread( JavaVM *vm, JNIEnv **jni, void *args )
{
	// First read current thread name.
	char threadName[16] = {0};	// max thread name length is 15.
	char commpath[64] = {0};
	OVR::OVR_sprintf( commpath, sizeof( commpath ), "/proc/%d/task/%d/comm", getpid(), gettid() );
	FILE * f = fopen( commpath, "r" );
	if ( f != NULL )
	{
		fread( threadName, 1, sizeof( threadName ) - 1, f );
		fclose( f );
		// Remove any trailing new-line characters.
		for ( int len = strlen( threadName ) - 1; len >= 0 && ( threadName[len] == '\n' || threadName[len] == '\r' ); len-- )
		{
			threadName[len] = '\0';
		}
	}
	// Attach the thread to the JVM.
	const jint rtn = vm->AttachCurrentThread( jni, args );
	if ( rtn != JNI_OK )
	{
		FAIL( "AttachCurrentThread returned %i", rtn );
	}
	// Restore the thread name after AttachCurrentThread() overwrites it.
	if ( threadName[0] != '\0' )
	{
		pthread_setname_np( pthread_self(), threadName );
	}
	return rtn;
}

jint ovr_DetachCurrentThread( JavaVM * vm )
{
	const jint rtn = vm->DetachCurrentThread();
	if ( rtn != JNI_OK )
	{
		FAIL( "DetachCurrentThread() returned %i", rtn );
	}
	return rtn;
}

jobject ovr_NewStringUTF( JNIEnv * jni, char const * str )
{
	return jni->NewStringUTF( str );
}

char const * ovr_GetStringUTFChars( JNIEnv * jni, jstring javaStr, jboolean * isCopy ) 
{
	char const * str = jni->GetStringUTFChars( javaStr, isCopy );
	return str;
}

jclass ovr_GetClassLoader( JNIEnv * jni, jobject contextObject )
{
	JavaClass contextClass( jni, jni->GetObjectClass( contextObject ) );
	jmethodID getClassLoaderMethodId = jni->GetMethodID( contextClass.GetJClass(), "getClassLoader", "()Ljava/lang/ClassLoader;" );
	jclass localClass = static_cast<jclass>( jni->CallObjectMethod( contextObject, getClassLoaderMethodId ) );

	if ( localClass == 0 )
	{
		FAIL( "getClassLoaderFailed failed" );
	}

	return localClass;
}

jclass ovr_GetLocalClassReferenceWithLoader( JNIEnv * jni, jobject classLoader, const char * className )
{
	JavaClass classLoaderClass( jni, jni->FindClass( "java/lang/ClassLoader" ) );
	jmethodID loadClassMethodId = jni->GetMethodID( classLoaderClass.GetJClass(), "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;" );

	JavaString classNameString( jni, jni->NewStringUTF( className ) );
	jclass localClass = static_cast<jclass>( jni->CallObjectMethod( classLoader, loadClassMethodId, classNameString.GetJString() ) );

	if ( localClass == 0 )
	{
		FAIL( "FindClass( %s ) failed", className );
	}

	return localClass;
}

jclass ovr_GetGlobalClassReferenceWithLoader( JNIEnv * jni, jobject classLoader, const char * className )
{
	JavaClass localClass( jni, ovr_GetLocalClassReferenceWithLoader( jni, classLoader, className ) );

	// Turn it into a global reference so it can be safely used on other threads.
	jclass globalClass = (jclass)jni->NewGlobalRef( localClass.GetJClass() );
	return globalClass;
}

// This can be called from any thread but does need the activity object.
jclass ovr_GetLocalClassReference( JNIEnv * jni, jobject activityObject, const char * className )
{
	JavaObject classLoaderObject( jni, ovr_GetClassLoader( jni, activityObject) );

	return ovr_GetLocalClassReferenceWithLoader( jni, classLoaderObject.GetJObject(), className );
}

// This can be called from any thread but does need the activity object.
jclass ovr_GetGlobalClassReference( JNIEnv * jni, jobject activityObject, const char * className )
{
	JavaClass localClass( jni, ovr_GetLocalClassReference( jni, activityObject, className ) );

	// Turn it into a global reference so it can be safely used on other threads.
	jclass globalClass = (jclass)jni->NewGlobalRef( localClass.GetJClass() );
	return globalClass;
}

jmethodID ovr_GetMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature )
{
	const jmethodID methodId = jni->GetMethodID( jniclass, name, signature );
	if ( !methodId )
	{
		FAIL( "couldn't get %s, %s", name, signature );
	}
	return methodId;
}

jmethodID ovr_GetStaticMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature )
{
	const jmethodID method = jni->GetStaticMethodID( jniclass, name, signature );
	if ( !method )
	{
		FAIL( "couldn't get %s, %s", name, signature );
	}
	return method;
}

const char * ovr_GetPackageCodePath( JNIEnv * jni, jobject activityObject, char * packageCodePath, int const maxLen )
{
	if ( packageCodePath == NULL || maxLen < 1 )
	{
		return packageCodePath;
	}

	packageCodePath[0] = '\0';

	JavaClass activityClass( jni, jni->GetObjectClass( activityObject ) );
	jmethodID getPackageCodePathId = jni->GetMethodID( activityClass.GetJClass(), "getPackageCodePath", "()Ljava/lang/String;" );
	if ( getPackageCodePathId == 0 )
	{
		LOG( "Failed to find getPackageCodePath on class %llu, object %llu",
				(long long unsigned int)activityClass.GetJClass(), (long long unsigned int)activityObject );
		return packageCodePath;
	}

	JavaUTFChars result( jni, (jstring)jni->CallObjectMethod( activityObject, getPackageCodePathId ) );
	if ( !jni->ExceptionOccurred() )
	{
		const char * path = result.ToStr();
		if ( path != NULL ) 
		{
			OVR::OVR_sprintf( packageCodePath, maxLen, "%s", path );
		}
	}
	else 
	{
		jni->ExceptionClear();
		LOG( "Cleared JNI exception" );
	}

	//LOG( "ovr_GetPackageCodePath() = '%s'", packageCodePath );
	return packageCodePath;
}

bool ovr_GetInstalledPackagePath( JNIEnv * jni, jobject activityObject, char const * packageName, 
		char * packagePath, int const packagePathSize )
{
	OVR_ASSERT( packagePath != NULL && packagePathSize > 1 && packageName != NULL && packageName[0] != '\0' );
	
	packagePath[0] = '\0';

	/// FIXME: a number of static functions on the Java VrActivity class should probably be moved to
	/// a utility class because they are still used for any application that uses VrAppFramework, 
	/// even if that application doesn't derive from VrActivity. 
	JavaClass vrActivityClass( jni, ovr_GetLocalClassReference( jni, activityObject, "com/oculus/vrappframework/VrActivity" ) );
	jmethodID getInstalledPackagePathId = ovr_GetStaticMethodID( jni, vrActivityClass.GetJClass(), 
			"getInstalledPackagePath", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;" );
	if ( getInstalledPackagePathId != NULL )
	{
		JavaString packageNameObj( jni, packageName );
		JavaUTFChars resultStr( jni, static_cast< jstring >( jni->CallStaticObjectMethod( vrActivityClass.GetJClass(), 
				getInstalledPackagePathId, activityObject, packageNameObj.GetJString() ) ) );
		if ( !jni->ExceptionOccurred() )
		{
			OVR::OVR_strcpy( packagePath, packagePathSize, resultStr.ToStr() );
			if ( packagePath[0] == '\0' )
			{
				return false;
			}
			return true;
		}
		else
		{
			WARN( "Exception occurred when calling getInstalledPackagePathId" );
			jni->ExceptionClear();
		}
	}
	return false;
}

const char * ovr_GetCurrentPackageName( JNIEnv * jni, jobject activityObject, char * packageName, int const maxLen )
{
	packageName[0] = '\0';

	JavaClass curActivityClass( jni, jni->GetObjectClass( activityObject ) );
	jmethodID getPackageNameId = jni->GetMethodID( curActivityClass.GetJClass(), "getPackageName", "()Ljava/lang/String;");
	if ( getPackageNameId != 0 )
	{
		JavaUTFChars result( jni, (jstring)jni->CallObjectMethod( activityObject, getPackageNameId ) );
		if ( !jni->ExceptionOccurred() )
		{
			const char * currentPackageName = result.ToStr();
			if ( currentPackageName != NULL )
			{
				OVR::OVR_sprintf( packageName, maxLen, "%s", currentPackageName );
			}
		}
		else
		{
			jni->ExceptionClear();
			LOG( "Cleared JNI exception" );
		}
	}
	//LOG( "ovr_GetCurrentPackageName() = %s", packageName );
	return packageName;
}

const char * ovr_GetCurrentActivityName( JNIEnv * jni, jobject activityObject, char * activityName, int const maxLen )
{
	activityName[0] = '\0';

	JavaClass curActivityClass( jni, jni->GetObjectClass( activityObject ) );
	jmethodID getClassMethodId = jni->GetMethodID( curActivityClass.GetJClass(), "getClass", "()Ljava/lang/Class;" );
	if ( getClassMethodId != 0 )
	{
		JavaObject classObj( jni, jni->CallObjectMethod( activityObject, getClassMethodId ) );
		JavaClass activityClass( jni, jni->GetObjectClass( classObj.GetJObject() ) );

		jmethodID getNameMethodId = jni->GetMethodID( activityClass.GetJClass(), "getName", "()Ljava/lang/String;" );
		if ( getNameMethodId != 0 )
		{
			JavaUTFChars utfCurrentClassName( jni, (jstring)jni->CallObjectMethod( classObj.GetJObject(), getNameMethodId ) );
			const char * currentClassName = utfCurrentClassName.ToStr();
			if ( currentClassName != NULL )
			{
				OVR::OVR_sprintf( activityName, maxLen, "%s", currentClassName );
			}
		}
	}

	//LOG( "ovr_GetCurrentActivityName() = %s", activityName );
	return activityName;
}

bool ovr_IsCurrentPackage( JNIEnv * jni, jobject activityObject, const char * packageName )
{
	char currentPackageName[128];
	ovr_GetCurrentPackageName( jni, activityObject, currentPackageName, sizeof( currentPackageName ) );
	const bool isCurrentPackage = ( strcasecmp( currentPackageName, packageName ) == 0 );
	//LOG( "ovr_IsCurrentPackage( %s ) = %s", packageName, isCurrentPackage ? "true" : "false" );
	return isCurrentPackage;
}

bool ovr_IsCurrentActivity( JNIEnv * jni, jobject activityObject, const char * activityName )
{
	char currentClassName[128];
	ovr_GetCurrentActivityName( jni, activityObject, currentClassName, sizeof( currentClassName ) );
	const bool isCurrentActivity = ( strcasecmp( currentClassName, activityName ) == 0 );
	//LOG( "ovr_IsCurrentActivity( %s ) = %s", activityName, isCurrentActivity ? "true" : "false" );
	return isCurrentActivity;
}

#else
jint ovr_AttachCurrentThread( JavaVM *vm, JNIEnv **jni, void *args )
{
	return 0;
}

jint ovr_DetachCurrentThread( JavaVM * vm )
{
	return 0;
}

jclass ovr_GetLocalClassReference( JNIEnv * jni, jobject activityObject, const char * className )
{
	return NULL;
}

// This can be called from any thread but does need the activity object.
jclass ovr_GetGlobalClassReference( JNIEnv * jni, jobject activityObject, const char * className )
{
	return NULL;
}

jmethodID ovr_GetStaticMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature )
{
	return NULL;
}

bool ovr_IsCurrentActivity( JNIEnv * jni, jobject activityObject, const char * activityName )
{
	return false;
}

bool ovr_IsCurrentPackage( JNIEnv * jni, jobject activityObject, const char * packageName )
{
	return false;
}

#endif
