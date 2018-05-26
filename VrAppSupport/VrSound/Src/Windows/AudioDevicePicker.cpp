/************************************************************************************

Filename    :   AudioDevicePicker.cpp
Content     :	Pick the audio device.
Created     :	March 6, 2016
Authors     :   Gloria Kennickell

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

*************************************************************************************/

#include "Kernel/OVR_LogUtils.h"

#include <mmdeviceapi.h>
#include <objbase.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <atlbase.h>

namespace OVR
{

DEFINE_PROPERTYKEY(PKEY_AudioEndpoint_Path, 0x9c119480, 0xddc2, 0x4954, 0xa1, 0x50, 0x5b, 0xd2, 0x40, 0xd4, 0x54, 0xad, 1);
EXTERN_C const PROPERTYKEY DECLSPEC_SELECTANY PKEY_AudioEndpoint_Path = { { 0x9c119480, 0xddc2, 0x4954, { 0xa1, 0x50, 0x5b, 0xd2, 0x40, 0xd4, 0x54, 0xad } }, 1 };

LPWSTR PickAudioOutDevice( LPWSTR deviceGuid )
{
	CoInitializeEx( NULL, COINIT_MULTITHREADED );
	CComPtr<IMMDeviceEnumerator> enumerator;

	HRESULT hr = ::CoCreateInstance( __uuidof( MMDeviceEnumerator ), NULL, CLSCTX_ALL,
		__uuidof( IMMDeviceEnumerator ), (void **)&enumerator );

	if ( hr != S_OK )
	{
		return NULL;
	}

	CComPtr<IMMDeviceCollection> devices;
	if ( enumerator->EnumAudioEndpoints( eRender, DEVICE_STATE_ACTIVE, &devices ) != S_OK )
	{
		return NULL;
	}

	UINT count;
	if ( devices->GetCount( &count ) != S_OK )
	{
		return NULL;
	}

	for ( UINT i = 0; i < count; ++i )
	{
		CComPtr<IMMDevice> device;
		if ( devices->Item( i, &device ) != S_OK )
		{
			continue;
		}

		CComPtr<IPropertyStore> properties;
		if ( device->OpenPropertyStore( STGM_READ, &properties ) != S_OK )
		{
			continue;
		}
		PROPVARIANT name;
		if ( properties->GetValue( PKEY_Device_FriendlyName, &name ) != S_OK )
		{
			continue;
		}
		PROPVARIANT path;
		if ( properties->GetValue( PKEY_AudioEndpoint_Path, &path ) != S_OK )
		{
			continue;
		}
		LPWSTR id;
		if ( device->GetId( &id ) != S_OK )
		{
			continue;
		}
		if ( wcscmp( id, deviceGuid ) == 0 )
		{
			return path.pwszVal;
		}
		::CoTaskMemFree( id );
	}

	return NULL;
}

}
