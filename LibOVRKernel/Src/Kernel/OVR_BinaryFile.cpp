/************************************************************************************

Filename    :   OVR_BinaryFile.cpp
Content     :   Simple helper class to read a binary file.
Created     :   Jun, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "OVR_BinaryFile.h"
#include "OVR_SysFile.h"

namespace OVR
{

BinaryReader::~BinaryReader()
{
	if ( Allocated )
	{
		OVR_FREE( const_cast< uint8_t* >( Data ) );
	}
}

BinaryReader::BinaryReader( const char * path, const char ** perror ) :
	Data( NULL ),
	Size( 0 ),
	Offset( 0 ),
	Allocated( true )
{
	SysFile f;
	if ( !f.Open( path, File::Open_Read, File::Mode_Read ) )
	{
		if ( perror != NULL )
		{
			*perror = "Failed to open file";
		}
		return;
	}

	Size = f.GetLength();
	Data = (uint8_t*) OVR_ALLOC( Size + 1 );
	int bytes = f.Read( (uint8_t *)Data, Size );
	if ( bytes != Size && perror != NULL )
	{
		*perror = "Failed to read file";
	}
	f.Close();
}

} // namespace OVR
