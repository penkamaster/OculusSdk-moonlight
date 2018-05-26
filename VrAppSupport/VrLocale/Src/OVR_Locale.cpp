/************************************************************************************

Filename    :   OVR_Locale.cpp
Content     :   Implementation of string localization for strings loaded at run-time.
Created     :   April 6, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#include "OVR_Locale.h"

#include <sys/stat.h>

#include "tinyxml2.h"
#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_Hash.h"
#include "Kernel/OVR_MemBuffer.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_LogUtils.h"
#include "Android/JniUtils.h"
#include "OVR_FileSys.h"

namespace OVR {

char const *	ovrLocale::LOCALIZED_KEY_PREFIX = "@string/";
size_t const	ovrLocale::LOCALIZED_KEY_PREFIX_LEN = OVR_strlen( LOCALIZED_KEY_PREFIX );

// DJB2 string hash function
static unsigned long DJB2Hash( char const * str )
{
    unsigned long hash = 5381;
    int c;

	for ( c = *str; c != 0; str++, c = *str )
    //while ( c = *str++ )
	{
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
    return hash;
}

//==============================================================
// Hash functor for OVR::String

template< class C >
class OvrStringHash
{
public:
	UPInt operator()( const C & data ) const
	{
		return DJB2Hash( data.ToCStr() );
	}
};

//==============================================================
// ovrLocaleInternal
class ovrLocaleInternal : public ovrLocale
{
public:
	static char const *	LOCALIZED_KEY_PREFIX;
	static OVR::UPInt	LOCALIZED_KEY_PREFIX_LEN;

#if defined( OVR_OS_ANDROID )
	ovrLocaleInternal( JNIEnv & jni_, jobject activity_, char const * name, char const * languageCode );
#else
	ovrLocaleInternal( char const * name, char const * languageCode );
#endif
	virtual ~ovrLocaleInternal();

	// returns the language code for this locale object
	virtual char const *	GetName() const { return Name.ToCStr(); }

	virtual char const *	GetLanguageCode() const { return LanguageCode.ToCStr(); }

	virtual bool			IsSystemDefaultLocale() const;

	virtual bool			LoadStringsFromAndroidFormatXMLFile( ovrFileSys & fileSys, char const * fileName );

	virtual bool			AddStringsFromAndroidFormatXMLBuffer( char const * name, char const * buffer, size_t const size );

	virtual bool			GetString( char const * key, char const * defaultStr, String & out ) const;

	virtual void			ReplaceLocalizedText( char const * inText, char * out, size_t const outSize ) const;

private:
#if defined( OVR_OS_ANDROID )
	JNIEnv &								jni;
	jobject									activityObject;
#endif

	typedef OvrStringHash< String >			HashFunctor;

	String									Name;			// user-specified locale name
	String									LanguageCode;	// system-specific locale name
	OVR::Hash< String, int, HashFunctor >	StringHash;
	Array< String	>						Strings;

private:
#if defined( OVR_OS_ANDROID )
	bool					GetStringJNI( char const * key, char const * defaultOut, String & out ) const;
#endif
};

char const *	ovrLocaleInternal::LOCALIZED_KEY_PREFIX = "@string/";
OVR::UPInt		ovrLocaleInternal::LOCALIZED_KEY_PREFIX_LEN = OVR_strlen( LOCALIZED_KEY_PREFIX );

//==============================
// ovrLocaleInternal::ovrLocaleInternal
#if defined( OVR_OS_ANDROID )
ovrLocaleInternal::ovrLocaleInternal( JNIEnv & jni_, jobject activity_, char const * name, char const * languageCode )
	: jni( jni_ )
	, activityObject( activity_ )
	, Name( name )
	, LanguageCode( languageCode )
{
}
#else
ovrLocaleInternal::ovrLocaleInternal( char const * name, char const * languageCode )
	: Name( name )
	, LanguageCode( languageCode )
{
}
#endif

//==============================
// ovrLocaleInternal::~ovrLocaleInternal
ovrLocaleInternal::~ovrLocaleInternal()
{
}

//==============================
// ovrLocaleInternal::IsSystemDefaultLocale
bool ovrLocaleInternal::IsSystemDefaultLocale() const
{
#if defined( OVR_OS_ANDROID )
	return OVR_stricmp( LanguageCode.ToCStr(), "en" ) == 0;
#else
	// FIXME: Implement
	return true;
#endif
}

static void GetValueFromNode( String & v, tinyxml2::XMLNode const * node )
{
	tinyxml2::XMLNode const * child = node->FirstChild();
	if ( child != nullptr )
	{
		GetValueFromNode( v, child );
	}
	else
	{
		v += node->Value();
	}
	tinyxml2::XMLNode const * sib = node->NextSibling();
	if ( sib != nullptr )
	{
		GetValueFromNode( v, sib );
	}
}

//==============================
// ovrLocaleInternal::AddStringsFromAndroidFormatXMLBuffer
bool ovrLocaleInternal::AddStringsFromAndroidFormatXMLBuffer( char const * name, char const * buffer, size_t const size )
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError error = doc.Parse( buffer, size );
	if ( error != tinyxml2::XML_NO_ERROR )
	{
		LOG( "ERROR: XML parse error %i parsing '%s'!", error, name );
		return false;
	}

	tinyxml2::XMLElement * root = doc.RootElement();
	if ( OVR_stricmp( root->Value(), "resources" ) != 0 )
	{
		LOG( "ERROR: Expected root value of 'resources', found '%s'!\n", root->Value() );
		return false;
	}

	tinyxml2::XMLElement const * curElement = root->FirstChildElement();
	for ( ; curElement != NULL; curElement = curElement->NextSiblingElement() )
	{
		if ( OVR_stricmp( curElement->Value(), "string" ) != 0 )
		{
			LOG( "WARNING: Expected element value 'string', found '%s'!\n", curElement->Value() );
			continue;
		}

		tinyxml2::XMLAttribute const * nameAttr = curElement->FindAttribute( "name" );

		String key = nameAttr->Value();

		String value;
		String decodedValue;

		tinyxml2::XMLNode const * childNode = curElement->FirstChild();
		if ( childNode != nullptr )
		{
			GetValueFromNode( value, childNode );
		}
		else
		{
			value = curElement->GetText();
		}

		// fix special encodings. Use GetFirstCharAt() and GetNextChar() to handle UTF-8.
		const char * in = value.ToCStr();
		uint32_t curChar = UTF8Util::DecodeNextChar( &in );
		while( curChar != 0 )
		{
			if ( curChar == '\\' )
			{
				uint32_t nextChar = UTF8Util::DecodeNextChar( &in );
				if ( nextChar == 0 )
				{
					break;
				}
				else if ( nextChar == 'n' )
				{
					curChar = '\n';
				}
				else if ( nextChar == '\r' )
				{
					curChar = '\r';
				}
				else 
				{
					if ( nextChar != '<' &&
					 nextChar != '>' &&
					 nextChar != '"' &&
					 nextChar != '\'' &&
					 nextChar != '&' )
					{
						LOG( "Unknown escape sequence '\\%x'", nextChar );
						decodedValue.AppendChar( curChar );
					}
					curChar = nextChar;
				}
			}
			else if ( curChar == '%' )
			{
				// if we find "%%", skip over the second '%' char because the localization pipeline bot is erroneously 
				// outputting doubled % format specifiers.
				const char * prev = in;
				uint32_t nextChar = UTF8Util::DecodeNextChar( &in );
				if ( nextChar != '%' )
				{
					// if it wasn't a double '%', then don't skip the next character
					in = prev;
				}
			}

			decodedValue.AppendChar( curChar );

			curChar = UTF8Util::DecodeNextChar( &in );
		}
		//LOG( "Name: '%s' = '%s'\n", key.ToCStr(), value.ToCStr() );

		int index = -1;
		if ( !StringHash.Get( key, &index ) )
		{
			StringHash.Add( key, Strings.GetSizeI() );
			Strings.PushBack( decodedValue );
		}
	}

	LOG( "Added %i strings from '%s'", Strings.GetSizeI(), name );

	return true;
}

//==============================
// ovrLocaleInternal::LoadStringsFromAndroidFormatXMLFile
bool ovrLocaleInternal::LoadStringsFromAndroidFormatXMLFile( ovrFileSys & fileSys, char const * fileName )
{
	MemBufferT< uint8_t > buffer;
	if ( !fileSys.ReadFile( fileName, buffer ) )
	{
		return false;
	}
	return AddStringsFromAndroidFormatXMLBuffer( fileName, reinterpret_cast< char const * > ( static_cast< uint8_t const * >( buffer) ), buffer.GetSize() );
}

#if defined( OVR_OS_ANDROID )
//==============================
// ovrLocale::GetStringJNI
// Get's a localized UTF-8-encoded string from the Android application's string table.
bool ovrLocaleInternal::GetStringJNI( char const * key, char const * defaultOut, String & out ) const
{

	//LOG( "Localizing key '%s'", key );
	// if the key doesn't start with KEY_PREFIX then it's not a valid key, just return
	// the key itself as the output text.
	if ( strstr( key, LOCALIZED_KEY_PREFIX ) != key )
	{
		out = defaultOut;
		LOG( "no prefix, localized to '%s'", out.ToCStr() );
		return true;
	}

	char const * realKey = key + LOCALIZED_KEY_PREFIX_LEN;
	//LOG( "realKey = %s", realKey );

	JavaClass vrLocaleClass( &jni, ovr_GetLocalClassReference( &jni, activityObject,
			"com/oculus/vrlocale/VrLocale" ) );
	jmethodID const getLocalizedStringId = ovr_GetStaticMethodID( &jni, vrLocaleClass.GetJClass(),
		"getLocalizedString", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;" );
	if ( getLocalizedStringId != NULL )
	{
		JavaString keyObj( &jni, realKey );
		JavaUTFChars resultStr( &jni, static_cast< jstring >( jni.CallStaticObjectMethod( vrLocaleClass.GetJClass(),
				getLocalizedStringId, activityObject, keyObj.GetJString() ) ) );
		if ( !jni.ExceptionOccurred() )
		{
			out = resultStr;
			if ( out.IsEmpty() )
			{
				out = defaultOut;
				LOG( "key not found, localized to '%s'", out.ToCStr() );
				return false;
			}

			//LOG( "localized to '%s'", out.ToCStr() );
			return true;
		}
		WARN( "Exception calling VrLocale.getLocalizedString" );
	}
	else
	{
		WARN( "Could not find VrLocale.getLocalizedString()" );
	}

	out = "JAVAERROR";
	OVR_ASSERT( false );	// the java code is missing getLocalizedString or an exception occured while calling it
	return false;
}
#endif

//==============================
// ovrLocaleInternal::GetString
bool ovrLocaleInternal::GetString( char const * key, char const * defaultStr, String & out ) const
{
	if ( key == NULL )
	{
		return false;
	}

	if ( strstr( key, LOCALIZED_KEY_PREFIX ) == key )
	{
		if ( Strings.GetSizeI() > 0 )
		{
			String realKey( key + LOCALIZED_KEY_PREFIX_LEN );
			int index = -1;
			if ( StringHash.Get( realKey, &index ) )
			{
				out = Strings[index];
				return true;
			}
		}
	}
#if defined( OVR_OS_ANDROID )
	// try instead to find the string via Android's resources. Ideally, we'd have combined these all
	// into our own hash, but enumerating application resources from library code on is problematic
	// on android
	if ( GetStringJNI( key, defaultStr, out ) )
	{
		return true;
	}
#endif
	out = defaultStr != NULL ? defaultStr : "";
	return false;
}

//==============================
// ovrLocaleInternal::ReplaceLocalizedText
void ovrLocaleInternal::ReplaceLocalizedText( char const * inText, char * out, size_t const outSize ) const
{
	char const * cur = strstr( inText, LOCALIZED_KEY_PREFIX );
	if ( cur == nullptr )
	{
		OVR_strcpy( out, outSize, inText );
		return;
	}

	const size_t MAX_AT_STRING_LEN = 256;
	size_t outOfs = 0;
	char const * last = inText;

	auto CopyChars = [] ( char * out, size_t outSize, size_t & outOfs, char const * src, size_t const count )
	{
		size_t remaining = outSize - outOfs - 1;
		size_t copyCount = count > remaining ? remaining : count;
		memcpy( &out[outOfs], src, copyCount );
		outOfs += copyCount;
		if ( count > remaining )
		{
			out[outOfs] = '\0';
			return false;
		}
		return true;
	};

	while( cur != nullptr )
	{
		// copy from the last to the current
		if ( !CopyChars( out, outSize, outOfs, last, cur - last ) )
		{
			return;
		}

		// scan ahead to find white space terminating the "@string/"
		size_t ofs = 0;
		char atString[MAX_AT_STRING_LEN];
		for ( ; cur[ofs] != '\0' && ofs < MAX_AT_STRING_LEN - 1; ++ofs )
		{
			if ( cur[ofs] == '\n' || cur[ofs] == '\r' || cur[ofs] == '\t' || cur[ofs] == ' ' )
			{
				break;
			}
			atString[ofs] = cur[ofs];
		}

		// terminate the @string
		atString[ofs] = '\0';
		// advance past the string
		cur += ofs;
		last = cur;

		// get the localized text
		String localized;
		GetString( atString, atString, localized );

		// copy localized text into the output buffer
		if ( !CopyChars( out, outSize, outOfs, localized.ToCStr(), OVR_strlen( localized.ToCStr() ) ) )
		{
			return;
		}

		cur = strstr( cur, LOCALIZED_KEY_PREFIX );
		if ( cur == nullptr )
		{
			// copy any remainder
			for ( size_t i = 0; last[i] != '\0' && outOfs < outSize - 1; ++i )
			{
				out[outOfs++] = last[i];
			}
			out[outOfs] = '\0';
			break;
		}
	}
}


//==============================================================================================
// ovrLocale
// static functions for managing the global instance to a ovrLocaleInternal object
//==============================================================================================

//==============================
// ovrLocale::Create
ovrLocale * ovrLocale::Create( JNIEnv & jni_, jobject activity_, char const * name, ovrFileSys * fileSys )
{
	LOG( "ovrLocale::Create - entered" );

	ovrLocale * localePtr = NULL;

#if defined( OVR_OS_ANDROID )
	// add the strings from the Android resource file
	JavaClass vrLocaleClass( &jni_, ovr_GetLocalClassReference( &jni_, activity_,
			"com/oculus/vrlocale/VrLocale" ) );
	if ( vrLocaleClass.GetJClass() == NULL )
	{
		LOG( "Couldn't find VrLocale class." );
	}
	jmethodID getCurrentLanguageMethodId = ovr_GetStaticMethodID( &jni_, vrLocaleClass.GetJClass(),
			"getCurrentLanguage", "()Ljava/lang/String;" );
	if ( getCurrentLanguageMethodId != NULL )
	{
		char const * languageCode = "en";
		JavaUTFChars utfCurrentLanguage( &jni_, (jstring)jni_.CallStaticObjectMethod( vrLocaleClass.GetJClass(), getCurrentLanguageMethodId ) );
		if ( jni_.ExceptionOccurred() )
		{
			WARN( "Exception occurred when calling getCurrentLanguage" );
			jni_.ExceptionClear();
		}
		else
		{
			languageCode = utfCurrentLanguage.ToStr();
		}
		localePtr = new ovrLocaleInternal( jni_, activity_, name, languageCode );
	}
	else
	{
		WARN( "Could not find VrLocale.getCurrentLanguage" );
	}
#elif defined( OVR_OS_WIN32 )
	OVR_UNUSED( &jni_ );
	OVR_UNUSED( activity_ );
	/// FIXME: we need to map Windows locales to the Android equivalents so we can load the corresponding locale file
#if 1
	char const * languageCode = "en";
#else
	char const * languageCode = "zh-rCN";
#endif
	localePtr = new ovrLocaleInternal( name, languageCode );

	auto AddLocale = [localePtr, fileSys] ( char const * languageCode )
	{
		char languageFolder[128] = "values";
		if ( OVR_stricmp( languageCode, "en" ) != 0 )
		{
			OVR_sprintf( languageFolder, sizeof( languageFolder ), "values-%s", languageCode );
		}

		if ( fileSys == nullptr )
		{
			LOG( "Null fileSys used when creating ovrLocale -- no string tables will be loaded!" );
		}
		else
		{
			char assetsFile[ovrFileSys::OVR_MAX_URI_LEN];
			OVR_sprintf( assetsFile, sizeof( assetsFile ), "apk:///res/%s/assets.xml", languageFolder );

			localePtr->LoadStringsFromAndroidFormatXMLFile( *fileSys, assetsFile );

			char stringsFile[ovrFileSys::OVR_MAX_URI_LEN];
			OVR_sprintf( stringsFile, sizeof( stringsFile ), "apk:///res/%s/strings.xml", languageFolder );

			localePtr->LoadStringsFromAndroidFormatXMLFile( *fileSys, stringsFile );
		}
	};

	AddLocale( languageCode );
	if ( OVR_strcmp( languageCode, "en" ) != 0 )
	{
		AddLocale( "en" );
	}
#else
	OVR_UNUSED( &jni_ );
	OVR_UNUSED( activity_ );
	localePtr = new ovrLocaleInternal( name, "en" );
#endif

	LOG( "ovrLocale::Create - exited" );
	return localePtr;
}

//==============================
// ovrLocale::Destroy
void ovrLocale::Destroy( ovrLocale * & localePtr )
{
	delete localePtr;
	localePtr = NULL;
}

//==============================
// ovrLocale::MakeStringIdFromUTF8
// Turns an arbitray ansi string into a string id.
// - Deletes any character that is not a space, letter or number.
// - Turn spaces into underscores.
// - Ignore contiguous spaces.
String ovrLocale::MakeStringIdFromUTF8( char const * str )
{
	enum eLastOutputType
	{
		LO_LETTER,
		LO_DIGIT,
		LO_SPACE,
		LO_MAX
	};
	eLastOutputType lastOutputType = LO_MAX;
	String out = LOCALIZED_KEY_PREFIX;
	char const * ptr = str;
	if ( strstr( str, LOCALIZED_KEY_PREFIX ) == str )
	{
		// skip UTF-8 chars... technically could just += LOCALIZED_KEY_PREFIX_LEN if the key prefix is only ANSI chars...
		for ( size_t i = 0; i < LOCALIZED_KEY_PREFIX_LEN; ++i )
		{
			UTF8Util::DecodeNextChar( &ptr );
		}
	}
	int n = static_cast< int >( UTF8Util::GetLength( ptr ) );
	for ( int i = 0; i < n; ++i )
	{
		uint32_t c = UTF8Util::DecodeNextChar( &ptr );
		if ( ( c >= '0' && c <= '9' ) )
		{
			if ( i == 0 )
			{
				// string identifiers in Android cannot start with a number because they
				// are also encoded as Java identifiers, so output an underscore first.
				out.AppendChar( '_' );
			}
			out.AppendChar( c );
			lastOutputType = LO_DIGIT;
		}
		else if ( ( c >= 'a' && c <= 'z' ) )
		{
			// just output the character
			out.AppendChar( c );
			lastOutputType = LO_LETTER;
		}
		else if ( ( c >= 'A' && c <= 'Z' ) )
		{
			// just output the character as lowercase
			out.AppendChar( c + 32 );
			lastOutputType = LO_LETTER;
		}
		else if ( c == 0x20 )
		{
			if ( lastOutputType != LO_SPACE )
			{
				out.AppendChar( '_' );
				lastOutputType = LO_SPACE;
			}
			continue;
		}
		// ignore everything else
	}
	return out;
}

//==============================
// ovrLocale::MakeStringIdFromANSI
// Turns an arbitray ansi string into a string id.
// - Deletes any character that is not a space, letter or number.
// - Turn spaces into underscores.
// - Ignore contiguous spaces.
String ovrLocale::MakeStringIdFromANSI( char const * str )
{
	enum eLastOutputType
	{
		LO_LETTER,
		LO_DIGIT,
		LO_SPACE,
		LO_PUNCTUATION,
		LO_MAX
	};
	eLastOutputType lastOutputType = LO_MAX;
	String out = LOCALIZED_KEY_PREFIX;
	char const * ptr = strstr( str, LOCALIZED_KEY_PREFIX ) == str ? str + LOCALIZED_KEY_PREFIX_LEN : str;
	OVR::UPInt n = OVR_strlen( ptr );
	for ( unsigned int i = 0; i < n; ++i )
	{
		unsigned char c = ptr[i];
		if ( ( c >= '0' && c <= '9' ) )
		{
			if ( i == 0 )
			{
				// string identifiers in Android cannot start with a number because they
				// are also encoded as Java identifiers, so output an underscore first.
				out.AppendChar( '_' );
			}
			out.AppendChar( c );
			lastOutputType = LO_DIGIT;
		}
		else if ( ( c >= 'a' && c <= 'z' ) )
		{
			// just output the character
			out.AppendChar( c );
			lastOutputType = LO_LETTER;
		}
		else if ( ( c >= 'A' && c <= 'Z' ) )
		{
			// just output the character as lowercase
			out.AppendChar( c + 32 );
			lastOutputType = LO_LETTER;
		}
		else if ( c == 0x20 )
		{
			if ( lastOutputType != LO_SPACE )
			{
				out.AppendChar( '_' );
				lastOutputType = LO_SPACE;
			}
			continue;
		}
		// ignore everything else
	}
	return out;
}

//==============================
// private_GetXliffFormattedString
// Supports up to 9 arguments and %s format only
// inXliffStr is intentionally not passed by reference because "va_start has undefined behavior with reference types"
static String private_GetXliffFormattedString( const StringDataPtr inXliffStr, ... )
{
	// format spec looks like: %1$s - we expect at least 3 chars after %
	const int MIN_NUM_EXPECTED_FORMAT_CHARS = 3;

	// If the passed in string is shorter than minimum expected xliff formatting, just return it
	if ( static_cast< int >( inXliffStr.GetSize() ) <= MIN_NUM_EXPECTED_FORMAT_CHARS )
	{
		return inXliffStr.ToCStr();
	}

	// Buffer that holds formatted return string
	StringBuffer retStrBuffer;

	char const * p = inXliffStr.ToCStr();
	for ( ; ; )
	{
		uint32_t charCode = UTF8Util::DecodeNextChar( &p );
		if ( charCode == '\0' )
		{
			break;
		}
		else if ( charCode == '%' )
		{
			// We found the start of the format specifier
			// Now check that there are at least three more characters which contain the format specification
			Array< uint32_t > formatSpec;
			for ( int count = 0; count < MIN_NUM_EXPECTED_FORMAT_CHARS; ++count )
			{
				uint32_t formatCharCode = UTF8Util::DecodeNextChar( &p );
				formatSpec.PushBack( formatCharCode );
			}

			OVR_ASSERT( formatSpec.GetSizeI() >= MIN_NUM_EXPECTED_FORMAT_CHARS );

			uint32_t desiredArgIdxChar = formatSpec.At( 0 );
			uint32_t dollarThing = formatSpec.At( 1 );
			uint32_t specifier = formatSpec.At( 2 );

			// Checking if it has supported xliff format specifier
			if ( ( desiredArgIdxChar >= '1' && desiredArgIdxChar <= '9' ) &&
				 ( dollarThing == '$' ) &&
				 ( specifier == 's' ) )
			{
				// Found format valid specifier, so processing entire format specifier.
				int desiredArgIdxint = desiredArgIdxChar - '0';

				va_list args;
				va_start( args, inXliffStr );

				// Loop till desired argument is found.
				for ( int j = 0; ; ++j )
				{
					const char * tempArg = va_arg( args, const char* );
					if ( j == ( desiredArgIdxint - 1 ) ) // found desired argument
					{
						retStrBuffer.AppendFormat( "%s", tempArg );
						break;
					}
				}

				va_end( args );
			}
			else
			{
				LOG( "%s has invalid xliff format - has unsupported format specifier.", inXliffStr.ToCStr() );
				return inXliffStr.ToCStr();
			}
		}
		else
		{
			retStrBuffer.AppendChar( charCode );
		}
	}

	return String( retStrBuffer );
}

//==============================
// ovrLocale::GetXliffFormattedString
String ovrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1 )
{
	return private_GetXliffFormattedString( StringDataPtr(inXliffStr), arg1 );
}

//==============================
// ovrLocale::GetXliffFormattedString
String ovrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2 )
{
	return private_GetXliffFormattedString( StringDataPtr(inXliffStr), arg1, arg2 );
}

//==============================
// ovrLocale::GetXliffFormattedString
OVR::String ovrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2, const char * arg3 )
{
	return private_GetXliffFormattedString( StringDataPtr(inXliffStr), arg1, arg2, arg3 );
}

//==============================
// ovrLocale::ToString
String ovrLocale::ToString( char const * fmt, float const f )
{
	char buffer[128];
	OVR_sprintf( buffer, 128, fmt, f );
	return String( buffer );
}

//==============================
// ovrLocale::ToString
String ovrLocale::ToString( char const * fmt, int const i )
{
	char buffer[128];
	OVR_sprintf( buffer, 128, fmt, i );
	return String( buffer );
}


} // namespace OVR
