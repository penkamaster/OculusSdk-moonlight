/************************************************************************************

Filename    :   ModelFile_OvrScene.cpp
Content     :   Model file loading glTF elements.
Created     :   December 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "ModelFileLoading.h"

namespace OVR {

#define GLTF_BINARY_MAGIC				( ( 'g' << 0 ) | ( 'l' << 8 ) | ( 'T' << 16 ) | ( 'F' << 24 ) )
#define GLTF_BINARY_VERSION				2
#define GLTF_BINARY_CHUNKTYPE_JSON		0x4E4F534A
#define GLTF_BINARY_CHUNKTYPE_BINARY	0x004E4942			

typedef struct glTFBinaryHeader
{
	uint32_t					magic;
	uint32_t					version;
	uint32_t					length;
} glTFBinaryHeader;



static uint8_t * ReadBufferFromZipFile( unzFile zfp, const uint8_t * fileData, const unz_file_info& finfo )
{
	const int size = finfo.uncompressed_size;
	uint8_t * buffer = nullptr;

	if ( unzOpenCurrentFile( zfp ) != UNZ_OK )
	{
		return buffer;
	}

	if ( finfo.compression_method == 0 && fileData != nullptr )
	{
		buffer = ( uint8_t * )fileData + unzGetCurrentFileZStreamPos64( zfp );
	}
	else
	{
		buffer = new uint8_t[size + 1];
		buffer[size] = '\0';	// always zero terminate text files

		if ( unzReadCurrentFile( zfp, buffer, size ) != size )
		{
			delete[] buffer;
			buffer = nullptr;
		}
	}

	return buffer;
}

static uint8_t * ReadFileBufferFromZipFile( unzFile zfp, const char * fileName, int& bufferLength, const uint8_t * fileData )
{
	for ( int ret = unzGoToFirstFile( zfp ); ret == UNZ_OK; ret = unzGoToNextFile( zfp ) )
	{
		unz_file_info finfo;
		char entryName[256];
		unzGetCurrentFileInfo( zfp, &finfo, entryName, sizeof( entryName ), nullptr, 0, nullptr, 0 );

		if ( OVR_stricmp( entryName, fileName ) == 0 )
		{
			bufferLength = finfo.uncompressed_size;
			uint8_t * buffer = ReadBufferFromZipFile( zfp, fileData, finfo );
			return buffer;
		}
	}

	bufferLength = 0;
	return nullptr;
}

static void ParseIntArray( int * elements, const int count, const JsonReader arrayNode )
{
	int i = 0;
	if ( arrayNode.IsArray() )
	{
		while ( !arrayNode.IsEndOfArray() && i < count )
		{
			const JSON * node = arrayNode.GetNextArrayElement();
			elements[i] = node->GetInt32Value();
			i++;
		}
	}

	for ( ; i < count; i++ )
	{
		elements[i] = 0;
	}
}

static void ParseFloatArray( float * elements, const int count, JsonReader arrayNode )
{
	int i = 0;
	if ( arrayNode.IsArray() )
	{
		while ( !arrayNode.IsEndOfArray() && i < count )
		{
			const JSON * node = arrayNode.GetNextArrayElement();
			elements[i] = node->GetFloatValue();
			i++;
		}
	}

	for ( ; i < count; i++ )
	{
		elements[i] = 0.0f;
	}
}

template< typename _type_ >
bool ReadSurfaceDataFromAccessor( Array< _type_ > & out, ModelFile & modelFile, const int index, const ModelAccessorType type, const int componentType, const int count )
{
	bool loaded = true;
	if ( index >= 0 )
	{
		if ( index >= modelFile.Accessors.GetSizeI() )
		{
			WARN( "Error: Invalid index on gltfPrimitive accessor %d %d", index, modelFile.Accessors.GetSizeI() );
			loaded = false;
		}

		const ModelAccessor * accessor = &( modelFile.Accessors[index] );

		if ( count >= 0 && accessor->count != count )
		{
			WARN( "Error: Invalid count on gltfPrimitive accessor %d %d %d", index, count, accessor->count );
			loaded = false;
		}
		if ( accessor->type != type )
		{
			WARN( "Error: Invalid type on gltfPrimitive accessor %d %d %d", index, type, accessor->type );
			loaded = false;
		}

		if ( accessor->componentType != componentType )
		{
			WARN( "Error: Invalid componentType on gltfPrimitive accessor %d %d %d", index, componentType, accessor->componentType );
			loaded = false;
		}

		int readStride = 0;
		if ( accessor->bufferView->byteStride > 0 )
		{
			if ( accessor->bufferView->byteStride != (int)sizeof( out[0] ) )
			{
				if ( (int)sizeof( out[0] ) > accessor->bufferView->byteStride )
				{
					WARN( "Error: bytestride is %d, thats smaller thn %d", accessor->bufferView->byteStride, ( int )sizeof( out[0] ) );
					loaded = false;
				}
				else
				{
					readStride = accessor->bufferView->byteStride;
				}
			}
		}

		const size_t offset = accessor->byteOffset + accessor->bufferView->byteOffset;
		const size_t copySize = accessor->count * sizeof( out[0] );

		if ( accessor->bufferView->buffer->byteLength < ( offset + copySize ) || accessor->bufferView->byteLength < ( copySize ) )
		{
			WARN( "Error: accessor requesting too much data in gltfPrimitive %d %d %d", index, ( int )accessor->bufferView->byteLength, ( int )( offset + copySize ) );
			loaded = false;
		}

		if ( loaded )
		{
			out.Resize( accessor->count );

			// #TODO this doesn't seem to be working, figure out why.
			if ( false && readStride > 0 )
			{
				for ( int i = 0; i < count; i++ )
				{
					memcpy( &out[i], accessor->bufferView->buffer->bufferData + offset + (readStride * i), sizeof( out[0] ) );
				}
			}
			else
			{
				memcpy( &out[0], accessor->bufferView->buffer->bufferData + offset, copySize );
			}
		}
	}

	return loaded;
}

// Requires the buffers and images to already be loaded in the model
bool LoadModelFile_glTF_Json( ModelFile & modelFile, const char * modelsJson, 
	const ModelGlPrograms & programs, const MaterialParms & materialParms,
	ModelGeo * outModelGeo )
{
	LOG( "LoadModelFile_glTF_Json parsing %s", modelFile.FileName.ToCStr() );

	LOGCPUTIME( "LoadModelFile_glTF_Json" );

	bool loaded = true;

	const char * error = nullptr;
	JSON * json = JSON::Parse( modelsJson, &error );
	if ( json == nullptr )
	{
		WARN( "LoadModelFile_glTF_Json: Error loading %s : %s", modelFile.FileName.ToCStr(), error );
		loaded = false;
	}
	else
	{
		const JsonReader models( json );
		if ( models.IsObject() )
		{
			if ( loaded )
			{ // ASSET
				const JsonReader asset( models.GetChildByName( "asset" ) );
				if ( !asset.IsObject() )
				{
					WARN( "Error: No asset on gltfSceneFile" );
					loaded = false;
				}
				String versionString = asset.GetChildStringByName( "version" );
				String minVersion = asset.GetChildStringByName( "minVersion" );
				if ( OVR_stricmp( versionString.ToCStr(), "2.0" ) != 0 && OVR_stricmp( minVersion.ToCStr(), "2.0" ) != 0 )
				{
					WARN( "Error: Invalid version number '%s' on gltfFile, currently only version 2.0 supported", versionString.ToCStr() );
					loaded = false;
				}
			} // END ASSET

			if ( loaded )
			{ // ACCESSORS
				LOGV( "Loading accessors" );
				const JsonReader accessors( models.GetChildByName( "accessors" ) );
				if ( accessors.IsArray() )
				{
					while ( !accessors.IsEndOfArray() && loaded)
					{
						int count = 0;
						const JsonReader accessor( accessors.GetNextArrayElement() );
						if ( accessor.IsObject() )
						{

							ModelAccessor newGltfAccessor;

							newGltfAccessor.name = accessor.GetChildStringByName( "name" );
							const int bufferView = accessor.GetChildInt32ByName( "bufferView" );
							newGltfAccessor.byteOffset = accessor.GetChildInt32ByName( "byteOffset" );
							newGltfAccessor.componentType = accessor.GetChildInt32ByName( "componentType" );
							newGltfAccessor.count = accessor.GetChildInt32ByName( "count" );
							const String type = accessor.GetChildStringByName( "type" );
							newGltfAccessor.normalized = accessor.GetChildBoolByName( "normalized" );

							if ( bufferView < 0 || bufferView >= ( const int )modelFile.BufferViews.GetSize() )
							{
								WARN( "Error: Invalid bufferView Index in gltfAccessor" );
								loaded = false;
							}

							int componentCount = 0;
							if ( OVR_stricmp( type.ToCStr(), "SCALAR" ) == 0 ) { newGltfAccessor.type = ACCESSOR_SCALAR; componentCount = 1; }
							else if ( OVR_stricmp( type.ToCStr(), "VEC2" ) == 0 ) { newGltfAccessor.type = ACCESSOR_VEC2; componentCount = 2; }
							else if ( OVR_stricmp( type.ToCStr(), "VEC3" ) == 0 ) { newGltfAccessor.type = ACCESSOR_VEC3; componentCount = 3; }
							else if ( OVR_stricmp( type.ToCStr(), "VEC4" ) == 0 ) { newGltfAccessor.type = ACCESSOR_VEC4; componentCount = 4; }
							else if ( OVR_stricmp( type.ToCStr(), "MAT2" ) == 0 ) { newGltfAccessor.type = ACCESSOR_MAT2; componentCount = 4; }
							else if ( OVR_stricmp( type.ToCStr(), "MAT3" ) == 0 ) { newGltfAccessor.type = ACCESSOR_MAT3; componentCount = 9; }
							else if ( OVR_stricmp( type.ToCStr(), "MAT4" ) == 0 ) { newGltfAccessor.type = ACCESSOR_MAT4; componentCount = 16; }
							else
							{
								WARN( "Error: Invalid type in gltfAccessor" );
								loaded = false;
							}

							const JSON * min = accessor.GetChildByName( "min" );
							const JSON * max = accessor.GetChildByName( "max" );
							if ( min != nullptr && max != nullptr )
							{
								switch ( newGltfAccessor.componentType )
								{
								case GL_BYTE:
								case GL_UNSIGNED_BYTE:
								case GL_SHORT:
								case GL_UNSIGNED_SHORT:
								case GL_UNSIGNED_INT:
									ParseIntArray( newGltfAccessor.intMin, componentCount, min );
									ParseIntArray( newGltfAccessor.intMax, componentCount, max );
									break;
								case GL_FLOAT:
									ParseFloatArray( newGltfAccessor.floatMin, componentCount, min );
									ParseFloatArray( newGltfAccessor.floatMax, componentCount, max );
									break;
								default:
									WARN( "Error: Invalid componentType in gltfAccessor" );
									loaded = false;
								}
								newGltfAccessor.minMaxSet = true;
							}

							newGltfAccessor.bufferView = &modelFile.BufferViews[bufferView];
							modelFile.Accessors.PushBack( newGltfAccessor );

							count++;
						}
					}
				}
			} // END ACCESSORS

			if ( loaded )
			{ // SAMPLERS
				LOGV( "Loading samplers" );
				const JsonReader samplers( models.GetChildByName( "samplers" ) );
				if ( samplers.IsArray() )
				{
					while ( !samplers.IsEndOfArray() && loaded)
					{
						const JsonReader sampler( samplers.GetNextArrayElement() );
						if ( sampler.IsObject() )
						{
							ModelSampler newGltfSampler;

							newGltfSampler.name = sampler.GetChildStringByName( "name" );
							newGltfSampler.magFilter = sampler.GetChildInt32ByName( "magFilter", GL_LINEAR );
							newGltfSampler.minFilter = sampler.GetChildInt32ByName( "minFilter", GL_NEAREST_MIPMAP_LINEAR );
							newGltfSampler.wrapS = sampler.GetChildInt32ByName( "wrapS", GL_REPEAT );
							newGltfSampler.wrapT = sampler.GetChildInt32ByName( "wrapT", GL_REPEAT );

							if ( newGltfSampler.magFilter != GL_NEAREST && newGltfSampler.magFilter != GL_LINEAR )
							{
								WARN( "Error: Invalid magFilter in gltfSampler" );
								loaded = false;
							}
							if ( newGltfSampler.minFilter != GL_NEAREST && newGltfSampler.minFilter != GL_LINEAR
								&& newGltfSampler.minFilter != GL_LINEAR_MIPMAP_NEAREST && newGltfSampler.minFilter != GL_NEAREST_MIPMAP_LINEAR
								&& newGltfSampler.minFilter != GL_LINEAR_MIPMAP_LINEAR )
							{
								WARN( "Error: Invalid minFilter in gltfSampler" );
								loaded = false;
							}
							if ( newGltfSampler.wrapS != GL_CLAMP_TO_EDGE && newGltfSampler.wrapS != GL_MIRRORED_REPEAT && newGltfSampler.wrapS != GL_REPEAT )
							{
								WARN( "Error: Invalid wrapS in gltfSampler" );
								loaded = false;
							}
							if ( newGltfSampler.wrapT != GL_CLAMP_TO_EDGE && newGltfSampler.wrapT != GL_MIRRORED_REPEAT && newGltfSampler.wrapT != GL_REPEAT )
							{
								WARN( "Error: Invalid wrapT in gltfSampler" );
								loaded = false;
							}

							modelFile.Samplers.PushBack( newGltfSampler );
						}
					}
				}

				// default sampler
				ModelSampler defaultGltfSampler;
				defaultGltfSampler.name = "Default_Sampler";
				modelFile.Samplers.PushBack( defaultGltfSampler );
			} // END SAMPLERS

			if ( loaded )
			{ // TEXTURES
				LOGV( "Loading textures" );
				const JsonReader textures( models.GetChildByName( "textures" ) );
				if ( textures.IsArray() && loaded )
				{
					while ( !textures.IsEndOfArray() )
					{
						const JsonReader texture( textures.GetNextArrayElement() );
						if ( texture.IsObject() )
						{
							ModelTextureWrapper newGltfTexture;

							newGltfTexture.name = texture.GetChildStringByName( "name" );
							const int sampler = texture.GetChildInt32ByName( "sampler", -1 );
							const int image = texture.GetChildInt32ByName( "source", -1 );

							if ( sampler < -1 || sampler >= modelFile.Samplers.GetSizeI() )
							{
								WARN( "Error: Invalid sampler Index in gltfTexture" );
								loaded = false;
							}

							if ( image < -1 || image >= modelFile.Textures.GetSizeI() )
							{
								WARN( "Error: Invalid source Index in gltfTexture" );
								loaded = false;
							}


							if ( sampler < 0 )
							{
								newGltfTexture.sampler = &modelFile.Samplers[modelFile.Samplers.GetSizeI() - 1];
							}
							else
							{
								newGltfTexture.sampler = &modelFile.Samplers[sampler];
							}
							if ( image < 0 )
							{
								newGltfTexture.image = nullptr;
							}
							else
							{
								newGltfTexture.image = &modelFile.Textures[image];
							}
							modelFile.TextureWrappers.PushBack( newGltfTexture );
						}
					}
				}
			} // END TEXTURES

			if ( loaded )
			{ // MATERIALS
				LOGV( "Loading materials" );
				const JsonReader materials( models.GetChildByName( "materials" ) );
				if ( materials.IsArray() && loaded )
				{
					while ( !materials.IsEndOfArray() )
					{
						const JsonReader material( materials.GetNextArrayElement() );
						if ( material.IsObject() )
						{
							ModelMaterial newGltfMaterial;

							// material
							newGltfMaterial.name = material.GetChildStringByName( "name" );

							const JSON * emissiveFactor = material.GetChildByName( "emissiveFactor" );
							if ( emissiveFactor != nullptr )
							{
								if ( emissiveFactor->GetItemCount() != 3 )
								{
									WARN( "Error: Invalid Itemcount on emissiveFactor for gltfMaterial" );
									loaded = false;
								}
								newGltfMaterial.emmisiveFactor.x = emissiveFactor->GetItemByIndex( 0 )->GetFloatValue();
								newGltfMaterial.emmisiveFactor.y = emissiveFactor->GetItemByIndex( 1 )->GetFloatValue();
								newGltfMaterial.emmisiveFactor.z = emissiveFactor->GetItemByIndex( 2 )->GetFloatValue();
							}

							const String alphaModeString = material.GetChildStringByName( "alphaMode", "OPAQUE" );
							if ( OVR_stricmp( alphaModeString.ToCStr(), "OPAQUE" ) == 0 ) { newGltfMaterial.alphaMode = ALPHA_MODE_OPAQUE; }
							else if ( OVR_stricmp( alphaModeString.ToCStr(), "MASK" ) == 0 ) { newGltfMaterial.alphaMode = ALPHA_MODE_MASK; }
							else if ( OVR_stricmp( alphaModeString.ToCStr(), "BLEND" ) == 0 ) { newGltfMaterial.alphaMode = ALPHA_MODE_BLEND; }
							else
							{
								WARN( "Error: Invalid alphaMode in gltfMaterial" );
								loaded = false;
							}

							newGltfMaterial.alphaCutoff = material.GetChildFloatByName( "alphaCutoff", 0.5f );
							newGltfMaterial.doubleSided = material.GetChildBoolByName( "doubleSided", false );

							//pbrMetallicRoughness
							const JsonReader pbrMetallicRoughness = material.GetChildByName( "pbrMetallicRoughness" );
							if ( pbrMetallicRoughness.IsObject() )
							{
								const JSON * baseColorFactor = pbrMetallicRoughness.GetChildByName( "baseColorFactor" );
								if ( baseColorFactor != nullptr )
								{
									if ( baseColorFactor->GetItemCount() != 4 )
									{
										WARN( "Error: Invalid Itemcount on baseColorFactor for gltfMaterial" );
										loaded = false;
									}
									newGltfMaterial.baseColorFactor.x = baseColorFactor->GetItemByIndex( 0 )->GetFloatValue();
									newGltfMaterial.baseColorFactor.y = baseColorFactor->GetItemByIndex( 1 )->GetFloatValue();
									newGltfMaterial.baseColorFactor.z = baseColorFactor->GetItemByIndex( 2 )->GetFloatValue();
									newGltfMaterial.baseColorFactor.w = baseColorFactor->GetItemByIndex( 3 )->GetFloatValue();
								}

								const JsonReader baseColorTexture = pbrMetallicRoughness.GetChildByName( "baseColorTexture" );
								if ( baseColorTexture.IsObject() )
								{
									int index = baseColorTexture.GetChildInt32ByName( "index", -1 );
									if ( index < 0 || index >= modelFile.TextureWrappers.GetSizeI() )
									{
										WARN( "Error: Invalid baseColorTexture index in gltfMaterial" );
										loaded = false;
									}
									newGltfMaterial.baseColorTextureWrapper = &modelFile.TextureWrappers[index];
								}

								newGltfMaterial.metallicFactor = pbrMetallicRoughness.GetChildFloatByName( "metallicFactor", 1.0f );
								newGltfMaterial.roughnessFactor = pbrMetallicRoughness.GetChildFloatByName( "roughnessFactor", 1.0f );

								const JsonReader metallicRoughnessTexture = pbrMetallicRoughness.GetChildByName( "metallicRoughnessTexture" );
								if ( metallicRoughnessTexture.IsObject() )
								{
									int index = metallicRoughnessTexture.GetChildInt32ByName( "index", -1 );
									if ( index < 0 || index >= modelFile.TextureWrappers.GetSizeI() )
									{
										WARN( "Error: Invalid metallicRoughnessTexture index in gltfMaterial" );
										loaded = false;
									}
									newGltfMaterial.metallicRoughnessTextureWrapper = &modelFile.TextureWrappers[index];
								}
							}

							//normalTexture
							const JsonReader normalTexture = material.GetChildByName( "normalTexture" );
							if ( normalTexture.IsObject() )
							{
								int index = normalTexture.GetChildInt32ByName( "index", -1 );
								if ( index < 0 || index >= modelFile.TextureWrappers.GetSizeI() )
								{
									WARN( "Error: Invalid normalTexture index in gltfMaterial" );
									loaded = false;
								}
								newGltfMaterial.normalTextureWrapper = &modelFile.TextureWrappers[index];
								newGltfMaterial.normalTexCoord = normalTexture.GetChildInt32ByName( "texCoord", 0 );
								newGltfMaterial.normalScale = normalTexture.GetChildFloatByName( "scale", 1.0f );
							}

							//occlusionTexture
							const JsonReader occlusionTexture = material.GetChildByName( "occlusionTexture" );
							if ( occlusionTexture.IsObject() )
							{
								int index = occlusionTexture.GetChildInt32ByName( "index", -1 );
								if ( index < 0 || index >= modelFile.TextureWrappers.GetSizeI() )
								{
									WARN( "Error: Invalid occlusionTexture index in gltfMaterial" );
									loaded = false;
								}
								newGltfMaterial.occlusionTextureWrapper = &modelFile.TextureWrappers[index];
								newGltfMaterial.occlusionTexCoord = occlusionTexture.GetChildInt32ByName( "texCoord", 0 );
								newGltfMaterial.occlusionStrength = occlusionTexture.GetChildFloatByName( "strength", 1.0f );
							}

							//emissiveTexture
							const JsonReader emissiveTexture = material.GetChildByName( "emissiveTexture" );
							if ( emissiveTexture.IsObject() )
							{
								int index = emissiveTexture.GetChildInt32ByName( "index", -1 );
								if ( index < 0 || index >= modelFile.TextureWrappers.GetSizeI() )
								{
									WARN( "Error: Invalid emissiveTexture index in gltfMaterial" );
									loaded = false;
								}
								newGltfMaterial.emissiveTextureWrapper = &modelFile.TextureWrappers[index];
							}

							modelFile.Materials.PushBack( newGltfMaterial );
						}
					}
					// Add a default material at the end of the list for primitives with an unspecified material.
					ModelMaterial defaultmaterial;
					modelFile.Materials.PushBack( defaultmaterial );
				}
			} // END MATERIALS

			if ( loaded )
			{ // MODELS (gltf mesh)
				LOGV( "Loading meshes" );
				const JsonReader meshes( models.GetChildByName( "meshes" ) );
				if ( meshes.IsArray() )
				{
					while ( !meshes.IsEndOfArray() && loaded )
					{
						const JsonReader mesh( meshes.GetNextArrayElement() );
						if ( mesh.IsObject() )
						{
							Model newGltfModel;

							newGltfModel.name = mesh.GetChildStringByName( "name" );
							// #TODO: implement morph weights
							const JsonReader weights( mesh.GetChildByName( "weights" ) );
							if ( weights.IsArray() )
							{
								while ( !weights.IsEndOfArray() )
								{
									const JSON * weight = weights.GetNextArrayElement();
									newGltfModel.weights.PushBack( weight->GetFloatValue() );
								}
							}

							{ // SURFACES (gltf primative)
								const JsonReader primitives( mesh.GetChildByName( "primitives" ) );
								if ( !primitives.IsArray() )
								{
									WARN( "Error: no primitives on gltfMesh" );
									loaded = false;
								}

								while ( !primitives.IsEndOfArray() && loaded)
								{
									const JsonReader primitive( primitives.GetNextArrayElement() );

									ModelSurface newGltfSurface;

									const int materialIndex = primitive.GetChildInt32ByName( "material", -1 );
									if ( materialIndex < 0 )
									{
										LOGV( "Using default for material on %s", newGltfModel.name.ToCStr() );
										newGltfSurface.material = &modelFile.Materials[modelFile.Materials.GetSizeI() - 1];
									}
									else if ( materialIndex >= modelFile.Materials.GetSizeI() )
									{
										WARN( "Error: Invalid materialIndex on gltfPrimitive" );
										loaded = false;
									}
									else
									{
										newGltfSurface.material = &modelFile.Materials[materialIndex];
									}

									const int mode = primitive.GetChildInt32ByName( "mode", 4 );
									if ( mode < GL_POINTS || mode > GL_TRIANGLE_FAN )
									{
										WARN( "Error: Invalid mode on gltfPrimitive" );
										loaded = false;
									}
									if ( mode != GL_TRIANGLES )
									{
										// #TODO: support modes other than triangle?
										WARN( "Error: Mode other then TRIANGLE (4) not currently supported on gltfPrimitive" );
										loaded = false;
									}

									// #TODO: implement morph targets

									const JsonReader attributes( primitive.GetChildByName( "attributes" ) );
									if ( !attributes.IsObject() )
									{
										WARN( "Error: no attributes on gltfPrimitive" );
										loaded = false;
									}

									TriangleIndex outGeoIndexOffset = 0;
									if ( outModelGeo != nullptr )
									{
										outGeoIndexOffset = static_cast< TriangleIndex >( ( *outModelGeo ).positions.GetSize() );
									}

									// VERTICES
									VertexAttribs attribs;

									{ // POSITION and BOUNDS
										const int positionIndex = attributes.GetChildInt32ByName( "POSITION", -1 );
										if ( positionIndex < 0 )
										{
											WARN( "Error: Invalid position index on gltfPrimitive" );
											loaded = false;
										}

										loaded = ReadSurfaceDataFromAccessor( attribs.position, modelFile, positionIndex, ACCESSOR_VEC3, GL_FLOAT, -1 );

										const ModelAccessor * positionAccessor = &modelFile.Accessors[positionIndex];
										if ( positionAccessor == nullptr )
										{
											WARN( "Error: Invalid positionAccessor on surface %s", newGltfSurface.surfaceDef.surfaceName.ToCStr() );
											loaded = false;
										}
										else if ( !positionAccessor->minMaxSet )
										{
											WARN( "Error: no min and max set on positionAccessor on surface %s", newGltfSurface.surfaceDef.surfaceName.ToCStr() );
											loaded = false;
										}
										else
										{
											Vector3f min;
											min.x = positionAccessor->floatMin[0];
											min.y = positionAccessor->floatMin[1];
											min.z = positionAccessor->floatMin[2];
											newGltfSurface.surfaceDef.geo.localBounds.AddPoint( min );

											Vector3f max;
											max.x = positionAccessor->floatMax[0];
											max.y = positionAccessor->floatMax[1];
											max.z = positionAccessor->floatMax[2];
											newGltfSurface.surfaceDef.geo.localBounds.AddPoint( max );
										}
									}

									const int numVertices = attribs.position.GetSizeI();
									if ( loaded ) { loaded = ReadSurfaceDataFromAccessor( attribs.normal, modelFile, attributes.GetChildInt32ByName( "NORMAL", -1 ), ACCESSOR_VEC3, GL_FLOAT, numVertices ); }
									// #TODO:  we have tangent as a vec3, the spec has it as a vec4.  so we will have to one off the loading of it.  skipping for now, since none of our shaders use tangent
									//if ( loaded ) { loaded = ReadSurfaceDataFromAccessor( attribs.tangent, modelFile, attributes.GetChildInt32ByName( "TANGENT", -1 ), ACCESSOR_VEC3, GL_FLOAT, numVertices ); }
									if ( loaded ) { loaded = ReadSurfaceDataFromAccessor( attribs.binormal, modelFile, attributes.GetChildInt32ByName( "BINORMAL", -1 ), ACCESSOR_VEC3, GL_FLOAT, numVertices ); }
									if ( loaded ) { loaded = ReadSurfaceDataFromAccessor( attribs.color, modelFile, attributes.GetChildInt32ByName( "COLOR", -1 ), ACCESSOR_VEC4, GL_FLOAT, numVertices ); }
									if ( loaded ) { loaded = ReadSurfaceDataFromAccessor( attribs.uv0, modelFile, attributes.GetChildInt32ByName( "TEXCOORD_0", -1 ), ACCESSOR_VEC2, GL_FLOAT, numVertices ); }
									if ( loaded ) { loaded = ReadSurfaceDataFromAccessor( attribs.uv1, modelFile, attributes.GetChildInt32ByName( "TEXCOORD_1", -1 ), ACCESSOR_VEC2, GL_FLOAT, numVertices ); }
									// #TODO:  TEXCOORD_2 is in the gltf spec, but we only support 2 uv sets. support more uv coordinates, skipping for now.
									//if ( loaded ) { loaded = ReadSurfaceDataFromAccessor( attribs.uv2, modelFile, attributes.GetChildInt32ByName( "TEXCOORD_2", -1 ), ACCESSOR_VEC2, GL_FLOAT, attribs.position.GetSizeI() ); }
									// #TODO: get weights of type unsigned_byte and unsigned_short working.
									if ( loaded ) { loaded = ReadSurfaceDataFromAccessor( attribs.jointWeights, modelFile, attributes.GetChildInt32ByName( "WEIGHTS_0", -1 ), ACCESSOR_VEC4, GL_FLOAT, numVertices ); }
									// WEIGHT_0 can be either GL_UNSIGNED_SHORT or GL_BYTE
									if ( loaded ) 
									{ 
										int jointIndex = attributes.GetChildInt32ByName( "JOINTS_0", -1 );
										if ( jointIndex >= 0 && jointIndex < modelFile.Accessors.GetSizeI() )
										{
											ModelAccessor & acc = modelFile.Accessors[jointIndex];
											if ( acc.componentType == GL_UNSIGNED_SHORT )
											{
												attribs.jointIndices.Resize( acc.count );
												for ( int accIndex = 0; accIndex < acc.count; accIndex++ )
												{
													attribs.jointIndices[accIndex].x = ( int )( ( unsigned short * )( acc.BufferData() ) )[accIndex * 4 + 0];
													attribs.jointIndices[accIndex].y = ( int )( ( unsigned short * )( acc.BufferData() ) )[accIndex * 4 + 1];
													attribs.jointIndices[accIndex].z = ( int )( ( unsigned short * )( acc.BufferData() ) )[accIndex * 4 + 2];
													attribs.jointIndices[accIndex].w = ( int )( ( unsigned short * )( acc.BufferData() ) )[accIndex * 4 + 3];
												}
											}
											else if ( acc.componentType == GL_BYTE )
											{
												attribs.jointIndices.Resize( acc.count );
												for ( int accIndex = 0; accIndex < acc.count; accIndex++ )
												{
													attribs.jointIndices[accIndex].x = ( int )( ( uint8_t * )( acc.BufferData() ) )[accIndex * 4 + 0];
													attribs.jointIndices[accIndex].y = ( int )( ( uint8_t * )( acc.BufferData() ) )[accIndex * 4 + 1];
													attribs.jointIndices[accIndex].z = ( int )( ( uint8_t * )( acc.BufferData() ) )[accIndex * 4 + 2];
													attribs.jointIndices[accIndex].w = ( int )( ( uint8_t * )( acc.BufferData() ) )[accIndex * 4 + 3];
												}
											}
											else if ( acc.componentType == GL_FLOAT )
											{ // not officially in spec, but it's what our exporter spits out.
												attribs.jointIndices.Resize( acc.count );
												for ( int accIndex = 0; accIndex < acc.count; accIndex++ )
												{
													attribs.jointIndices[accIndex].x = ( int )( ( float * )( acc.BufferData() ) )[accIndex * 4 + 0];
													attribs.jointIndices[accIndex].y = ( int )( ( float * )( acc.BufferData() ) )[accIndex * 4 + 1];
													attribs.jointIndices[accIndex].z = ( int )( ( float * )( acc.BufferData() ) )[accIndex * 4 + 2];
													attribs.jointIndices[accIndex].w = ( int )( ( float * )( acc.BufferData() ) )[accIndex * 4 + 3];
												}
											}
											else
											{
												WARN( "invalid component type %d on joints_0 accessor on model %s", acc.componentType, modelFile.FileName.ToCStr() );
												loaded = false;
											}
										}
									}

									// TRIANGLES
									Array< TriangleIndex > indices;
									const int indicesIndex = primitive.GetChildInt32ByName( "indices", -1 );
									if ( indicesIndex < 0 || indicesIndex >= modelFile.Accessors.GetSizeI() )
									{
										WARN( "Error: Invalid indices index on gltfPrimitive" );
										loaded = false;
									}

									if ( modelFile.Accessors[indicesIndex].componentType != GL_UNSIGNED_SHORT )
									{
										WARN( "Error: Currently, only componentType of %d supported for indices, %d requested", GL_UNSIGNED_SHORT, modelFile.Accessors[indicesIndex].componentType );
										loaded = false;
									}

									if ( loaded ) { ReadSurfaceDataFromAccessor( indices, modelFile, primitive.GetChildInt32ByName( "indices", -1 ), ACCESSOR_SCALAR, GL_UNSIGNED_SHORT, -1 ); }

									newGltfSurface.surfaceDef.geo.Create( attribs, indices );
									
									

									// Create the uniform buffer for storing the joint matrices.
									if ( attribs.jointIndices.GetSizeI() > 0 )
									{
										newGltfSurface.surfaceDef.graphicsCommand.uniformJoints.Create( GLBUFFER_TYPE_UNIFORM, attribs.jointIndices.GetSize() * sizeof( Matrix4f ), nullptr );
									}
									
									bool skinned = ( attribs.jointIndices.GetSize() == attribs.position.GetSize() &&
										attribs.jointWeights.GetSize() == attribs.position.GetSize() );
									 
									if ( outModelGeo != nullptr )
									{
										for ( int i = 0; i < indices.GetSizeI(); ++i )
										{
											( *outModelGeo ).indices.PushBack( indices[i] + outGeoIndexOffset );
										}
									}

									// CREATE COMMAND BUFFERS.
									if ( newGltfSurface.material->alphaMode == ALPHA_MODE_MASK )
									{
										// #TODO: implement ALPHA_MODE_MASK if we need it.
										// Just blend because alpha testing is rather expensive.
										WARN( "gltfAlphaMode ALPHA_MODE_MASK requested, doing ALPHA_MODE_BLEND instead" );
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.depthMaskEnable = false;
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
									}
									else if ( newGltfSurface.material->alphaMode == ALPHA_MODE_BLEND || materialParms.Transparent )
									{
										if ( materialParms.Transparent&& newGltfSurface.material->alphaMode != ALPHA_MODE_BLEND )
										{
											WARN( "gltfAlphaMode is %d but treating at ALPHA_MODE_BLEND due to materialParms.Transparent", newGltfSurface.material->alphaMode );
										}
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.depthMaskEnable = false;
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
									}
									// #TODO: GLTF doesn't have a concept of an ADDITIVE mode.  maybe it should?
									//else if ( newGltfSurface.material->alphaMode == MATERIAL_TYPE_ADDITIVE )
									//{
									//	newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
									//	newGltfSurface.surfaceDef.graphicsCommand.GpuState.depthMaskEnable = false;
									//	newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendSrc = GL_ONE;
									//	newGltfSurface.surfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE;
									//}
									else if ( newGltfSurface.material->alphaMode == ALPHA_MODE_OPAQUE )
									{
										// default GpuState;
									}

									if ( newGltfSurface.material->baseColorTextureWrapper != nullptr )
									{
										newGltfSurface.surfaceDef.graphicsCommand.uniformTextures[0] = newGltfSurface.material->baseColorTextureWrapper->image->texid;

										if ( newGltfSurface.material->emissiveTextureWrapper != nullptr )
										{
											newGltfSurface.surfaceDef.graphicsCommand.numUniformTextures = 2;
											if ( programs.ProgBaseColorEmissivePBR == nullptr )
											{
												FAIL( "No ProgBaseColorEmissivePBR set" );
											}
											newGltfSurface.surfaceDef.graphicsCommand.uniformTextures[1] = newGltfSurface.material->emissiveTextureWrapper->image->texid;

											newGltfSurface.surfaceDef.graphicsCommand.uniformSlots[0] = 3;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][0] = newGltfSurface.material->baseColorFactor.x;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][1] = newGltfSurface.material->baseColorFactor.y;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][2] = newGltfSurface.material->baseColorFactor.z;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][3] = newGltfSurface.material->baseColorFactor.w;

											newGltfSurface.surfaceDef.graphicsCommand.uniformSlots[1] = 3;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[1][0] = newGltfSurface.material->emmisiveFactor.x;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[1][1] = newGltfSurface.material->emmisiveFactor.y;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[1][2] = newGltfSurface.material->emmisiveFactor.z;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[1][3] = 0.0f;

											if ( skinned )
											{
												newGltfSurface.surfaceDef.graphicsCommand.Program = *programs.ProgSkinnedBaseColorEmissivePBR;
												newGltfSurface.surfaceDef.surfaceName = "ProgSkinnedBaseColorEmissivePBR";
											}
											else
											{
												newGltfSurface.surfaceDef.graphicsCommand.Program = *programs.ProgBaseColorEmissivePBR;
												newGltfSurface.surfaceDef.surfaceName = "ProgBaseColorEmissivePBR";
											}
										}
										else
										{
											newGltfSurface.surfaceDef.graphicsCommand.numUniformTextures = 1;
											if ( programs.ProgBaseColorPBR == nullptr )
											{
												FAIL( "No ProgBaseColorPBR set" );
											}

											newGltfSurface.surfaceDef.graphicsCommand.uniformSlots[0] = 2;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][0] = newGltfSurface.material->baseColorFactor.x;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][1] = newGltfSurface.material->baseColorFactor.y;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][2] = newGltfSurface.material->baseColorFactor.z;
											newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][3] = newGltfSurface.material->baseColorFactor.w;

											if ( skinned )
											{
												newGltfSurface.surfaceDef.graphicsCommand.Program = *programs.ProgSkinnedBaseColorPBR;
												newGltfSurface.surfaceDef.surfaceName = "ProgSkinnedBaseColorPBR";
											}
											else
											{
												newGltfSurface.surfaceDef.graphicsCommand.Program = *programs.ProgBaseColorPBR;
												newGltfSurface.surfaceDef.surfaceName = "ProgBaseColorPBR";
											}
										}
									}
									else
									{
										newGltfSurface.surfaceDef.graphicsCommand.numUniformTextures = 0;
										if ( programs.ProgSimplePBR == nullptr )
										{
											FAIL( "No ProgSimplePBR set" );
										}

										newGltfSurface.surfaceDef.graphicsCommand.uniformSlots[0] = 1;
										newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][0] = newGltfSurface.material->baseColorFactor.x;
										newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][1] = newGltfSurface.material->baseColorFactor.y;
										newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][2] = newGltfSurface.material->baseColorFactor.z;
										newGltfSurface.surfaceDef.graphicsCommand.uniformValues[0][3] = newGltfSurface.material->baseColorFactor.w;

										if ( skinned )
										{
											newGltfSurface.surfaceDef.graphicsCommand.Program = *programs.ProgSkinnedSimplePBR;
											newGltfSurface.surfaceDef.surfaceName = "ProgSkinnedSimplePBR";
										}
										else
										{
											newGltfSurface.surfaceDef.graphicsCommand.Program = *programs.ProgSimplePBR;
											newGltfSurface.surfaceDef.surfaceName = "ProgSimplePBR";
										}
									}

									if ( materialParms.PolygonOffset )
									{
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.polygonOffsetEnable = true;
									}

									if ( newGltfSurface.material->doubleSided )
									{
										newGltfSurface.surfaceDef.graphicsCommand.GpuState.cullEnable = false;
									}
									newGltfModel.surfaces.PushBack( newGltfSurface );
								}
							} // END SURFACES
							modelFile.Models.PushBack( newGltfModel );
						}
					}
				}
			} // END MODELS

			if ( loaded )
			{ // CAMERAS
			  // #TODO: best way to expose cameras to apps?  
				LOGV( "Loading cameras" );
				const JsonReader cameras( models.GetChildByName( "cameras" ) );
				if ( cameras.IsArray() && loaded )
				{
					while ( !cameras.IsEndOfArray() )
					{
						const JsonReader camera( cameras.GetNextArrayElement() );
						if ( camera.IsObject() )
						{
							ModelCamera newGltfCamera;

							newGltfCamera.name = camera.GetChildStringByName( "name" );

							const String cameraTypeString = camera.GetChildStringByName( "type" );
							if ( OVR_stricmp( cameraTypeString.ToCStr(), "perspective" ) == 0 )
							{
								newGltfCamera.type = MODEL_CAMERA_TYPE_PERSPECTIVE;
							}
							else if ( OVR_stricmp( cameraTypeString.ToCStr(), "orthographic" ) == 0 )
							{
								newGltfCamera.type = MODEL_CAMERA_TYPE_ORTHOGRAPHIC;
							}
							else { 
								WARN( "Error: Invalid camera type on gltfCamera %s", cameraTypeString.ToCStr() );
								loaded = false;
							}

							if ( newGltfCamera.type == MODEL_CAMERA_TYPE_ORTHOGRAPHIC )
							{
								const JsonReader orthographic( camera.GetChildByName( "orthographic" ) );
								if ( !orthographic.IsObject() )
								{
									WARN( "Error: No orthographic object on orthographic gltfCamera" );
									loaded = false;
								}
								newGltfCamera.orthographic.magX = orthographic.GetChildFloatByName( "xmag" );
								newGltfCamera.orthographic.magY = orthographic.GetChildFloatByName( "ymag" );
								newGltfCamera.orthographic.nearZ = orthographic.GetChildFloatByName( "znear" );
								newGltfCamera.orthographic.farZ = orthographic.GetChildFloatByName( "zfar" );
								if ( newGltfCamera.orthographic.magX <= 0.0f
									|| newGltfCamera.orthographic.magY <= 0.0f
									|| newGltfCamera.orthographic.nearZ <= 0.0f
									|| newGltfCamera.orthographic.farZ <= newGltfCamera.orthographic.nearZ )
								{
									WARN( "Error: Invalid data in orthographic gltfCamera" );
									loaded = false;
								}
							}
							else // MODEL_CAMERA_TYPE_PERSPECTIVE
							{
								const JsonReader perspective( camera.GetChildByName( "perspective" ) );
								if ( !perspective.IsObject() )
								{
									WARN( "Error: No perspective object on perspective gltfCamera" );
									loaded = false;
								}
								newGltfCamera.perspective.aspectRatio = perspective.GetChildFloatByName( "aspectRatio" );
								const float yfov = perspective.GetChildFloatByName( "yfov" );
								newGltfCamera.perspective.fovDegreesX = ( 180.0f / 3.14159265358979323846f ) * 2.0f * atanf( tanf( yfov * 0.5f ) * newGltfCamera.perspective.aspectRatio );
								newGltfCamera.perspective.fovDegreesY = ( 180.0f / 3.14159265358979323846f ) * yfov;
								newGltfCamera.perspective.nearZ = perspective.GetChildFloatByName( "znear" );
								newGltfCamera.perspective.farZ = perspective.GetChildFloatByName( "zfar", 10000.0f );
								if ( newGltfCamera.perspective.fovDegreesX <= 0.0f
									|| newGltfCamera.perspective.fovDegreesY <= 0.0f
									|| newGltfCamera.perspective.nearZ <= 0.0f
									|| newGltfCamera.perspective.farZ <= 0.0f )
								{
									WARN( "Error: Invalid data in perspective gltfCamera" );
									loaded = false;
								}
							}
							modelFile.Cameras.PushBack( newGltfCamera );
						}
					}
				}
			} // END CAMERAS

			if ( loaded )
			{ // NODES
				LOGV( "Loading nodes" );
				const JSON * pNodes = models.GetChildByName( "nodes" );
				const JsonReader nodes( pNodes );
				if ( nodes.IsArray() && loaded )
				{
					modelFile.Nodes.Resize( pNodes->GetItemCount() );

					int nodeIndex = 0;
					while ( !nodes.IsEndOfArray() )
					{
						const JsonReader node( nodes.GetNextArrayElement() );
						if ( node.IsObject() )
						{
							ModelNode * pGltfNode = &modelFile.Nodes[nodeIndex];

							// #TODO: implement morph weights

							pGltfNode->name = node.GetChildStringByName( "name" );
							const JsonReader matrixReader = node.GetChildByName( "matrix" );
							if ( matrixReader.IsArray() )
							{
								Matrix4f matrix;
								ParseFloatArray( matrix.M[0], 16, matrixReader );
								matrix.Transpose();
								// TRANSLATION
								pGltfNode->translation = matrix.GetTranslation();
								// SCALE
								pGltfNode->scale.x = sqrtf( matrix.M[0][0] * matrix.M[0][0] + matrix.M[0][1] * matrix.M[0][1] + matrix.M[0][2] * matrix.M[0][2] );
								pGltfNode->scale.y = sqrtf( matrix.M[1][0] * matrix.M[1][0] + matrix.M[1][1] * matrix.M[1][1] + matrix.M[1][2] * matrix.M[1][2] );
								pGltfNode->scale.z = sqrtf( matrix.M[2][0] * matrix.M[2][0] + matrix.M[2][1] * matrix.M[2][1] + matrix.M[2][2] * matrix.M[2][2] );
								// ROTATION
								const float rcpScaleX = RcpSqrt( matrix.M[0][0] * matrix.M[0][0] + matrix.M[0][1] * matrix.M[0][1] + matrix.M[0][2] * matrix.M[0][2] );
								const float rcpScaleY = RcpSqrt( matrix.M[1][0] * matrix.M[1][0] + matrix.M[1][1] * matrix.M[1][1] + matrix.M[1][2] * matrix.M[1][2] );
								const float rcpScaleZ = RcpSqrt( matrix.M[2][0] * matrix.M[2][0] + matrix.M[2][1] * matrix.M[2][1] + matrix.M[2][2] * matrix.M[2][2] );
								const float m[9] =
								{
									matrix.M[0][0] * rcpScaleX, matrix.M[0][1] * rcpScaleX, matrix.M[0][2] * rcpScaleX,
									matrix.M[1][0] * rcpScaleY, matrix.M[1][1] * rcpScaleY, matrix.M[1][2] * rcpScaleY,
									matrix.M[2][0] * rcpScaleZ, matrix.M[2][1] * rcpScaleZ, matrix.M[2][2] * rcpScaleZ
								};
								if ( m[0 * 3 + 0] + m[1 * 3 + 1] + m[2 * 3 + 2] > 0.0f )
								{
									float t = +m[0 * 3 + 0] + m[1 * 3 + 1] + m[2 * 3 + 2] + 1.0f;
									float s = RcpSqrt( t ) * 0.5f;
									pGltfNode->rotation.w = s * t;
									pGltfNode->rotation.z = ( m[0 * 3 + 1] - m[1 * 3 + 0] ) * s;
									pGltfNode->rotation.y = ( m[2 * 3 + 0] - m[0 * 3 + 2] ) * s;
									pGltfNode->rotation.x = ( m[1 * 3 + 2] - m[2 * 3 + 1] ) * s;
								}
								else if ( m[0 * 3 + 0] > m[1 * 3 + 1] && m[0 * 3 + 0] > m[2 * 3 + 2] )
								{
									float t = +m[0 * 3 + 0] - m[1 * 3 + 1] - m[2 * 3 + 2] + 1.0f;
									float s = RcpSqrt( t ) * 0.5f;
									pGltfNode->rotation.x = s * t;
									pGltfNode->rotation.y = ( m[0 * 3 + 1] + m[1 * 3 + 0] ) * s;
									pGltfNode->rotation.z = ( m[2 * 3 + 0] + m[0 * 3 + 2] ) * s;
									pGltfNode->rotation.w = ( m[1 * 3 + 2] - m[2 * 3 + 1] ) * s;
								}
								else if ( m[1 * 3 + 1] > m[2 * 3 + 2] )
								{
									float t = -m[0 * 3 + 0] + m[1 * 3 + 1] - m[2 * 3 + 2] + 1.0f;
									float s = RcpSqrt( t ) * 0.5f;
									pGltfNode->rotation.y = s * t;
									pGltfNode->rotation.x = ( m[0 * 3 + 1] + m[1 * 3 + 0] ) * s;
									pGltfNode->rotation.w = ( m[2 * 3 + 0] - m[0 * 3 + 2] ) * s;
									pGltfNode->rotation.z = ( m[1 * 3 + 2] + m[2 * 3 + 1] ) * s;
								}
								else
								{
									float t = -m[0 * 3 + 0] - m[1 * 3 + 1] + m[2 * 3 + 2] + 1.0f;
									float s = RcpSqrt( t ) * 0.5f;
									pGltfNode->rotation.z = s * t;
									pGltfNode->rotation.w = ( m[0 * 3 + 1] - m[1 * 3 + 0] ) * s;
									pGltfNode->rotation.x = ( m[2 * 3 + 0] + m[0 * 3 + 2] ) * s;
									pGltfNode->rotation.y = ( m[1 * 3 + 2] + m[2 * 3 + 1] ) * s;
								}
							}

							const JSON * rotation = node.GetChildByName( "rotation" );
							if ( rotation != nullptr )
							{
								pGltfNode->rotation.x = rotation->GetItemByIndex( 0 )->GetFloatValue();
								pGltfNode->rotation.y = rotation->GetItemByIndex( 1 )->GetFloatValue();
								pGltfNode->rotation.z = rotation->GetItemByIndex( 2 )->GetFloatValue();
								pGltfNode->rotation.w = rotation->GetItemByIndex( 3 )->GetFloatValue();
							}

							const JSON * scale = node.GetChildByName( "scale" );
							if ( scale != nullptr )
							{
								pGltfNode->scale.x = scale->GetItemByIndex( 0 )->GetFloatValue();
								pGltfNode->scale.y = scale->GetItemByIndex( 1 )->GetFloatValue();
								pGltfNode->scale.z = scale->GetItemByIndex( 2 )->GetFloatValue();
							}

							const JSON * translation = node.GetChildByName( "translation" );
							if ( translation != nullptr )
							{
								pGltfNode->translation.x = translation->GetItemByIndex( 0 )->GetFloatValue();
								pGltfNode->translation.y = translation->GetItemByIndex( 1 )->GetFloatValue();
								pGltfNode->translation.z = translation->GetItemByIndex( 2 )->GetFloatValue();
							}

							pGltfNode->skinIndex = node.GetChildInt32ByName( "skin", -1 );

							int cameraIndex = node.GetChildInt32ByName( "camera", -1 );
							if ( cameraIndex >= 0 )
							{
								if ( cameraIndex >= modelFile.Cameras.GetSizeI() )
								{
									WARN( "Error: Invalid camera index %d on gltfNode", cameraIndex );
									loaded = false;
								}
								pGltfNode->camera = &modelFile.Cameras[cameraIndex];
							}

							int meshIndex = node.GetChildInt32ByName( "mesh", -1 );
							if ( meshIndex >= 0 )
							{
								if ( meshIndex >= modelFile.Models.GetSizeI() )
								{
									WARN( "Error: Invalid Mesh index %d on gltfNode", meshIndex );
									loaded = false;
								}
								pGltfNode->model = &modelFile.Models[meshIndex];
							}

							Matrix4f localTransform;
							CalculateTransformFromRTS( &localTransform, pGltfNode->rotation, pGltfNode->translation, pGltfNode->scale );
							pGltfNode->SetLocalTransform( localTransform );

							const JsonReader children = node.GetChildByName( "children" );
							if ( children.IsArray() )
							{

								while ( !children.IsEndOfArray() )
								{

									const JSON* child = children.GetNextArrayElement();
									int childIndex = child->GetInt32Value();
									
									if ( childIndex < 0 || childIndex >= modelFile.Nodes.GetSizeI() )
									{
										WARN( "Error: Invalid child node index %d for %d in gltfNode", childIndex, nodeIndex );
										loaded = false;
									}
									
									pGltfNode->children.PushBack( childIndex );
									modelFile.Nodes[childIndex].parentIndex = nodeIndex;
								}
							}

							nodeIndex++;
						}
					}
				}
			} // END NODES

			if ( loaded )
			{ // ANIMATIONS
				LOGV( "loading Animations" );
				const JSON * animationsJSON = models.GetChildByName( "animations" );
				const JsonReader animations = animationsJSON;
				if ( animations.IsArray() )
				{
					int animationCount = 0;
					while ( !animations.IsEndOfArray() && loaded )
					{
						modelFile.Animations.Resize( animationsJSON->GetArraySize() );
						const JsonReader animation( animations.GetNextArrayElement() );
						if ( animation.IsObject() )
						{
							ModelAnimation & modelAnimation = modelFile.Animations[animationCount];

							modelAnimation.name = animation.GetChildStringByName( "name" );

							// ANIMATION SAMPLERS
							const JsonReader samplers = animation.GetChildByName( "samplers" );
							if ( samplers.IsArray() )
							{
								while ( !samplers.IsEndOfArray() && loaded )
								{
									ModelAnimationSampler modelAnimationSampler;
									const JsonReader sampler = samplers.GetNextArrayElement();
									if ( sampler.IsObject() )
									{
										int inputIndex = sampler.GetChildInt32ByName( "input", -1 );
										if ( inputIndex < 0 || inputIndex >= modelFile.Accessors.GetSizeI() )
										{
											WARN( "bad input index %d on sample on %s", inputIndex, modelAnimation.name.ToCStr() );
											loaded = false;
										}
										else
										{
											modelAnimationSampler.input = &modelFile.Accessors[inputIndex];
											if ( modelAnimationSampler.input->componentType != GL_FLOAT )
											{
												WARN( "animation sampler input not of type GL_FLOAT on '%s'", modelAnimation.name.ToCStr() );
												loaded = false;
											}
										}

										int outputIndex = sampler.GetChildInt32ByName( "output", -1 );
										if ( outputIndex < 0 || outputIndex >= modelFile.Accessors.GetSizeI() )
										{
											WARN( "bad input outputIndex %d on sample on %s", outputIndex, modelAnimation.name.ToCStr() );
											loaded = false;
										}
										else
										{
											modelAnimationSampler.output = &modelFile.Accessors[outputIndex];
										}

										String interpolation = sampler.GetChildStringByName( "interpolation", "LINEAR" );
										if ( OVR_stricmp( interpolation.ToCStr(), "LINEAR" ) == 0 )
										{
											modelAnimationSampler.interpolation = MODEL_ANIMATION_INTERPOLATION_LINEAR;
										}
										else if ( OVR_stricmp( interpolation.ToCStr(), "STEP" ) == 0 )
										{
											modelAnimationSampler.interpolation = MODEL_ANIMATION_INTERPOLATION_STEP;
										}
										else if ( OVR_stricmp( interpolation.ToCStr(), "CATMULLROMSPLINE" ) == 0 )
										{
											modelAnimationSampler.interpolation = MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE;
										}
										else if ( OVR_stricmp( interpolation.ToCStr(), "CUBICSPLINE" ) == 0 )
										{
											modelAnimationSampler.interpolation = MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE;
										}
										else
										{
											WARN( "Error: Invalid interpolation type '%s' on sampler on animtion '%s'", interpolation.ToCStr(), modelAnimation.name.ToCStr() );
											loaded = false;
										}

										if ( loaded )
										{
											if ( modelAnimationSampler.interpolation == MODEL_ANIMATION_INTERPOLATION_LINEAR || modelAnimationSampler.interpolation == MODEL_ANIMATION_INTERPOLATION_STEP )
											{
												if ( modelAnimationSampler.input->count != modelAnimationSampler.output->count )
												{
													WARN( "input and output have different counts on sampler  on animation '%s'", modelAnimation.name.ToCStr() );
													loaded = false;
												}
												if ( modelAnimationSampler.input->count < 2 )
												{
													WARN( "invalid number of samples on animation sampler input %d '%s'", modelAnimationSampler.input->count, modelAnimation.name.ToCStr() );
													loaded = false;
												}
											}
											else if ( modelAnimationSampler.interpolation == MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE )
											{
												if ( ( modelAnimationSampler.input->count + 2 ) != modelAnimationSampler.output->count )
												{
													WARN( "input and output have invalid counts on sampler on animation '%s'", modelAnimation.name.ToCStr() );
													loaded = false;
												}
												if ( modelAnimationSampler.input->count < 4 )
												{
													WARN( "invalid number of samples on animation sampler input %d '%s'", modelAnimationSampler.input->count, modelAnimation.name.ToCStr() );
													loaded = false;
												}
											}
											else if ( modelAnimationSampler.interpolation == MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE )
											{
												if ( modelAnimationSampler.input->count != ( modelAnimationSampler.output->count * 3) )
												{
													WARN( "input and output have invalid counts on sampler on animation '%s'", modelAnimation.name.ToCStr() );
													loaded = false;
												}
												if ( modelAnimationSampler.input->count < 2 )
												{
													WARN( "invalid number of samples on animation sampler input %d '%s'", modelAnimationSampler.input->count, modelAnimation.name.ToCStr() );
													loaded = false;
												}
											}
											else
											{
												WARN( "unkown animaiton interpolation on '%s'", modelAnimation.name.ToCStr() );
												loaded = false;
											}
										}

										modelAnimation.samplers.PushBack( modelAnimationSampler );
									}
									else
									{
										WARN( "bad sampler on '%s'", modelAnimation.name.ToCStr() );
										loaded = false;
									}
								}
							}
							else
							{
								WARN( "bad samplers on '%s'", modelAnimation.name.ToCStr() );
								loaded = false;
							} // END ANIMATION SAMPLERS

							  // ANIMATION CHANNELS
							const JsonReader channels = animation.GetChildByName( "channels" );
							if ( channels.IsArray() )
							{
								while ( !channels.IsEndOfArray() && loaded )
								{
									const JsonReader channel = channels.GetNextArrayElement();
									if ( channel.IsObject() )
									{
										ModelAnimationChannel modelAnimationChannel;

										int samplerIndex = channel.GetChildInt32ByName( "sampler", -1 );
										if ( samplerIndex < 0 || samplerIndex >= modelAnimation.samplers.GetSizeI() )
										{
											WARN( "bad samplerIndex %d on channel on %s", samplerIndex, modelAnimation.name.ToCStr() );
											loaded = false;
										}
										else
										{
											modelAnimationChannel.sampler = &modelAnimation.samplers[samplerIndex];
										}

										const JsonReader target = channel.GetChildByName( "target" );
										if ( target.IsObject() )
										{
											// not required so -1 means do not do animation.  
											int nodeIndex = target.GetChildInt32ByName( "node", -1 );
											if ( nodeIndex >= modelFile.Nodes.GetSizeI() )
											{
												WARN( "bad nodeIndex %d on target on '%s'", nodeIndex, modelAnimation.name.ToCStr() );
												loaded = false;
											}
											else
											{
												modelAnimationChannel.nodeIndex = nodeIndex;
											}

											String path = target.GetChildStringByName( "path" );

											if ( OVR_stricmp( path.ToCStr(), "translation" ) == 0 )
											{
												modelAnimationChannel.path = MODEL_ANIMATION_PATH_TRANSLATION;
											}
											else if ( OVR_stricmp( path.ToCStr(), "rotation" ) == 0 )
											{
												modelAnimationChannel.path = MODEL_ANIMATION_PATH_ROTATION;
											}
											else if ( OVR_stricmp( path.ToCStr(), "scale" ) == 0 )
											{
												modelAnimationChannel.path = MODEL_ANIMATION_PATH_SCALE;
											}
											else if ( OVR_stricmp( path.ToCStr(), "weights" ) == 0 )
											{
												modelAnimationChannel.path = MODEL_ANIMATION_PATH_WEIGHTS;
											}
											else
											{
												WARN( " bad path '%s' on target on '%s'", path.ToCStr(), modelAnimation.name.ToCStr() );
												loaded = false;
											}
										}
										else
										{
											WARN( "bad target object on '%s'", modelAnimation.name.ToCStr() );
											loaded = false;
										}


										modelAnimation.channels.PushBack( modelAnimationChannel );
									}
									else
									{
										WARN( "bad channel on '%s'", modelAnimation.name.ToCStr() );
										loaded = false;
									}
								}
							}
							else
							{
								WARN( "bad channels on '%s'", modelAnimation.name.ToCStr() );
								loaded = false;
							} // END ANIMATION CHANNELS

							if ( loaded )
							{ // ANIMATION TIMELINES
								// create the timelines
								for ( int i = 0; i < modelFile.Animations.GetSizeI(); i++ )
								{
									for ( int j = 0; j < modelFile.Animations[i].samplers.GetSizeI(); j++ )
									{
										// if there isn't already a timeline with this accessor, create a new one.
										ModelAnimationSampler & sampler = modelFile.Animations[i].samplers[j];
										bool foundTimeLine = false;
										for ( int timeLineIndex = 0; timeLineIndex < modelFile.AnimationTimeLines.GetSizeI(); timeLineIndex++ )
										{
											if ( modelFile.AnimationTimeLines[timeLineIndex].accessor == sampler.input )
											{
												foundTimeLine = true;
												sampler.timeLineIndex = timeLineIndex;
												break;
											}
										}
										
										if ( !foundTimeLine )
										{
											ModelAnimationTimeLine timeline;
											timeline.Initialize( sampler.input );
											if ( modelFile.AnimationTimeLines.GetSizeI() == 0 )
											{
												modelFile.animationStartTime = timeline.startTime;
												modelFile.animationEndTime = timeline.endTime;
											}
											else
											{
												modelFile.animationStartTime = Alg::Min( modelFile.animationStartTime, timeline.startTime );
												modelFile.animationEndTime = Alg::Max( modelFile.animationEndTime, timeline.endTime );
											}

											modelFile.AnimationTimeLines.PushBack( timeline );
											sampler.timeLineIndex = modelFile.AnimationTimeLines.GetSizeI() - 1;
										}
									}
								}
							} // END ANIMATION TIMELINES
							
							animationCount++;
						}
						else
						{
							WARN( "bad animation object in animations" );
							loaded = false;
						}
					}
				}
			} // END ANIMATIONS

			if ( loaded )
			{ // SKINS
				LOGV( "Loading skins" );
				const JsonReader skins( models.GetChildByName( "skins" ) );
				if ( skins.IsArray() )
				{
					while ( !skins.IsEndOfArray() && loaded )
					{
						const JsonReader skin( skins.GetNextArrayElement() );
						if ( skin.IsObject() )
						{
							ModelSkin newSkin;

							newSkin.name = skin.GetChildStringByName( "name" );
							newSkin.skeletonRootIndex = skin.GetChildInt32ByName( "skeleton", -1 );
							int bindMatricesAccessorIndex = skin.GetChildInt32ByName( "inverseBindMatrices", -1 );
							if ( bindMatricesAccessorIndex >= modelFile.Accessors.GetSizeI() )
							{
								WARN( "inverseBindMatrices %d higher then number of accessors on model: %s", bindMatricesAccessorIndex, modelFile.FileName.ToCStr() );
								loaded = false;
							}
							else if ( bindMatricesAccessorIndex >= 0 )
							{
								ModelAccessor & acc = modelFile.Accessors[bindMatricesAccessorIndex];
								newSkin.inverseBindMatricesAccessor = &modelFile.Accessors[bindMatricesAccessorIndex];
								for ( int i = 0; i < acc.count; i++ )
								{
									Matrix4f matrix;
									memcpy( matrix.M[0], ( ( float * )( acc.BufferData() ) ) + i  * 16, sizeof( float ) * 16 );
									matrix.Transpose();
									newSkin.inverseBindMatrices.PushBack( matrix );
								}
							}

							const JsonReader joints = skin.GetChildByName( "joints" );
							if ( joints.IsArray() )
							{
								while ( !joints.IsEndOfArray() && loaded )
								{
									int jointIndex = joints.GetNextArrayInt32( -1 );
									if ( jointIndex < 0 || jointIndex >= modelFile.Nodes.GetSizeI() )
									{
										WARN( "bad jointindex %d on skin on model: %s", jointIndex, modelFile.FileName.ToCStr() );
										loaded = false;
									}
									newSkin.jointIndexes.PushBack( jointIndex );
								}
							} 
							else
							{
								WARN( "no joints on skin on model: %s", modelFile.FileName.ToCStr() );
								loaded = false;
							}

							if ( newSkin.jointIndexes.GetSizeI() > MAX_JOINTS )
							{
								WARN( "%d joints on skin on model: %s, currently only %d allowed ", newSkin.jointIndexes.GetSizeI(), modelFile.FileName.ToCStr(), MAX_JOINTS );
								loaded = false;
							}

							modelFile.Skins.PushBack( newSkin );
						}
						else
						{
							WARN( "bad skin on model: %s", modelFile.FileName.ToCStr() );
							loaded = false;
						}
					}
				}
				
			} // END SKINS

			if ( loaded )
			{ // verify skin indexes on nodes
				for ( int i = 0; i < modelFile.Nodes.GetSizeI(); i++ )
				{
					if ( modelFile.Nodes[i].skinIndex > modelFile.Skins.GetSizeI() )
					{
						WARN( "bad skin index %d on node %d on model: %s", modelFile.Nodes[i].skinIndex, i, modelFile.FileName.ToCStr() );
						loaded = false;
					}
				}
			}

			if ( loaded )
			{ // SCENES
				LOGV( "Loading scenes" );
				const JsonReader scenes( models.GetChildByName( "scenes" ) );
				if ( scenes.IsArray() )
				{
					while ( !scenes.IsEndOfArray() && loaded )
					{
						const JsonReader scene( scenes.GetNextArrayElement() );
						if ( scene.IsObject() )
						{
							ModelSubScene newGltfScene;

							newGltfScene.name = scene.GetChildStringByName( "name" );

							const JsonReader nodes = scene.GetChildByName( "nodes" );
							if ( nodes.IsArray() )
							{
								while ( !nodes.IsEndOfArray() )
								{
									const int nodeIndex = nodes.GetNextArrayInt32();
									if ( nodeIndex < 0 || nodeIndex >= modelFile.Nodes.GetSizeI() )
									{
										WARN( "Error: Invalid nodeIndex %d in Model", nodeIndex );
										loaded = false;
									}
									newGltfScene.nodes.PushBack( nodeIndex );
								}
							}
							modelFile.SubScenes.PushBack( newGltfScene );
						}
					}
				}

				// Calculate the nodes global transforms;
				for ( int i = 0; i < modelFile.SubScenes.GetSizeI(); i++ )
				{
					for ( int j = 0; j < modelFile.SubScenes[i].nodes.GetSizeI(); j++ )
					{
						modelFile.Nodes[modelFile.SubScenes[i].nodes[j]].RecalculateGlobalTransform( modelFile );
					}
				}
			} // END SCENES

			if ( loaded )
			{
				const int sceneIndex = models.GetChildInt32ByName( "scene", -1 );
				if ( sceneIndex >= 0 )
				{
					if ( sceneIndex >= modelFile.SubScenes.GetSizeI() )
					{
						WARN( "Error: Invalid initial scene index %d on gltfFile", sceneIndex );
						loaded = false;
					}
					modelFile.SubScenes[sceneIndex].visible = true;
				}
			}

			// print out the scene info
			if ( loaded )
			{
				LOGV( "Model Loaded:     '%s'", modelFile.FileName.ToCStr() );
				LOGV( "\tBuffers        : %d", modelFile.Buffers.GetSizeI() );
				LOGV( "\tBufferViews    : %d", modelFile.BufferViews.GetSizeI() );
				LOGV( "\tAccessors      : %d", modelFile.Accessors.GetSizeI() );
				LOGV( "\tTextures       : %d", modelFile.Textures.GetSizeI() );
				LOGV( "\tTextureWrappers: %d", modelFile.TextureWrappers.GetSizeI() );
				LOGV( "\tMaterials      : %d", modelFile.Materials.GetSizeI() );
				LOGV( "\tModels         : %d", modelFile.Models.GetSizeI() );
				LOGV( "\tCameras        : %d", modelFile.Cameras.GetSizeI() );
				LOGV( "\tNodes          : %d", modelFile.Nodes.GetSizeI() );
				LOGV( "\tAnimations     : %d", modelFile.Animations.GetSizeI() );
				LOGV( "\tAnimationTimeLines: %d", modelFile.AnimationTimeLines.GetSizeI() );
				LOGV( "\tSkins          : %d", modelFile.Skins.GetSizeI() );
				LOGV( "\tSubScenes      : %d", modelFile.SubScenes.GetSizeI() );
			}
			else
			{
				WARN( "Could not load model '%s'", modelFile.FileName.ToCStr() );
			}

			// #TODO: what to do with our collision?  One possible answer is extras on the data tagging certain models as collision.
			// Collision Model
			// Ground Collision Model
			// Ray-Trace Model
		}
		else
		{
			loaded = false;
		}
	

		json->Release();
	}

	return loaded;
}

// A gltf directory zipped up into an ovrscene file.
bool LoadModelFile_glTF_OvrScene( ModelFile * modelFilePtr, unzFile zfp, const char * fileName,
	const char * fileData, const int fileDataLength,
	const ModelGlPrograms & programs,
	const MaterialParms & materialParms,
	ModelGeo * outModelGeo )
{

	ModelFile & modelFile = *modelFilePtr;

	// Since we are doing a zip file, we are going to parse through the zip file many times to find the different data points.
	const char * gltfJson = nullptr;
	int gltfJsonLength = 0;
	{
		LOGCPUTIME( "Loading GLTF file" );
		for ( int ret = unzGoToFirstFile( zfp ); ret == UNZ_OK; ret = unzGoToNextFile( zfp ) )
		{
			unz_file_info finfo;
			char entryName[256];
			unzGetCurrentFileInfo( zfp, &finfo, entryName, sizeof( entryName ), nullptr, 0, nullptr, 0 );
			const size_t entryLength = strlen( entryName );
			const char * extension = ( entryLength >= 5 ) ? &entryName[entryLength - 5] : entryName;

			if ( OVR_stricmp( extension, ".gltf" ) == 0 )
			{
				LOGV( "found %s", entryName );
				const int size = finfo.uncompressed_size;
				uint8_t * buffer = ReadBufferFromZipFile( zfp, ( const uint8_t * )fileData, finfo );

				if ( buffer == nullptr )
				{
					WARN( "LoadModelFile_glTF_OvrScene:Failed to read %s from %s", entryName, fileName );
					continue;
				}

				if ( gltfJson == nullptr )
				{
					gltfJson = ( const char * )buffer;
					gltfJsonLength = size;
				}
				else
				{
					WARN( "LoadModelFile_glTF_OvrScene: multiple .gltf files found %s", fileName );
					delete[] buffer;
					continue;
				}
			}
		}
	}

	bool loaded = true;

	const char * error = nullptr;
	JSON * json = JSON::Parse( gltfJson, &error );
	if ( json == nullptr )
	{
		WARN( "LoadModelFile_glTF_OvrScene: Error loading %s : %s", modelFilePtr->FileName.ToCStr(), error );
		loaded = false;
	}
	else
	{
		const JsonReader models( json );
		if ( models.IsObject() )
		{
			// Buffers BufferViews and Images need access to the data location, in this case the zip file.   
			//	after they are loaded it should be identical wether the input is a zip file, a folder structure or a bgltf file.
			if ( loaded )
			{ // BUFFERS
				LOGCPUTIME( "Loading buffers" );
				// gather all the buffers, and try to load them from the zip file.
				const JsonReader buffers( models.GetChildByName( "buffers" ) );
				if ( buffers.IsArray() )
				{
					while ( !buffers.IsEndOfArray() && loaded )
					{
						const JsonReader bufferReader( buffers.GetNextArrayElement() );
						if ( bufferReader.IsObject() )
						{
							ModelBuffer newGltfBuffer;

							const String name = bufferReader.GetChildStringByName( "name" );
							const String uri = bufferReader.GetChildStringByName( "uri" );
							newGltfBuffer.byteLength = bufferReader.GetChildInt32ByName( "byteLength", -1 );

							// #TODO: proper uri reading.  right now, assuming its a file name.
							if ( OVR_stricmp( uri.GetExtension().ToCStr(), ".bin" ) != 0 )
							{
								// #TODO: support loading buffers from data other then a bin file.  i.e. inline buffers etc.
								WARN( "Loading buffers other then bin files currently unsupported" );
								loaded = false;
							}
							int bufferLength = 0;
							uint8_t * tempbuffer = ReadFileBufferFromZipFile( zfp, uri.ToCStr(), bufferLength, ( const uint8_t * )fileData );
							if ( tempbuffer == nullptr )
							{
								WARN( "could not load buffer for gltfBuffer" );
								newGltfBuffer.bufferData = nullptr;
								loaded = false;
							}
							else
							{
								// ensure the buffer is aligned.
								newGltfBuffer.bufferData = (uint8_t *)(new float[bufferLength / 4 + 1]);
								memcpy( newGltfBuffer.bufferData, tempbuffer, bufferLength );
								newGltfBuffer.bufferData[bufferLength] = '\0';
							}

							if ( newGltfBuffer.byteLength > ( size_t )bufferLength )
							{
								WARN( "%d byteLength > bufferLength loading gltfBuffer %d", (int)newGltfBuffer.byteLength, bufferLength );
								loaded = false;
							}

							const char * bufferName;
							if ( name.GetLength() > 0 )
							{
								bufferName = name.ToCStr();
							}
							else
							{
								bufferName = uri.ToCStr();
							}

							newGltfBuffer.name = bufferName;

							modelFile.Buffers.PushBack( newGltfBuffer );
						}
					}
				}
			} // END BUFFERS

			if ( loaded )
			{ // BUFFERVIEW
				LOGV( "Loading bufferviews" );
				const JsonReader bufferViews( models.GetChildByName( "bufferViews" ) );
				if ( bufferViews.IsArray() )
				{
					while ( !bufferViews.IsEndOfArray() && loaded )
					{
						const JsonReader bufferview( bufferViews.GetNextArrayElement() );
						if ( bufferview.IsObject() )
						{
							ModelBufferView newBufferView;

							newBufferView.name = bufferview.GetChildStringByName( "name" );
							const int buffer = bufferview.GetChildInt32ByName( "buffer" );
							newBufferView.byteOffset = bufferview.GetChildInt32ByName( "byteOffset" );
							newBufferView.byteLength = bufferview.GetChildInt32ByName( "byteLength" );
							newBufferView.byteStride = bufferview.GetChildInt32ByName( "byteStride" );
							newBufferView.target = bufferview.GetChildInt32ByName( "target" );

							if ( buffer < 0 || buffer >= ( const int )modelFile.Buffers.GetSize() )
							{
								WARN( "Error: Invalid buffer Index in gltfBufferView" );
								loaded = false;
							}
							if ( newBufferView.byteStride < 0 || newBufferView.byteStride > 255 )
							{
								WARN( "Error: Invalid byeStride in gltfBufferView" );
								loaded = false;
							}
							if ( newBufferView.target < 0 )
							{
								WARN( "Error: Invalid target in gltfBufferView" );
								loaded = false;
							}

							newBufferView.buffer = &modelFile.Buffers[buffer];
							modelFile.BufferViews.PushBack( newBufferView );
						}
					}
				}
			} // END BUFFERVIEWS

			if ( loaded )
			{ // IMAGES
				LOGCPUTIME( "Loading image textures" );
				// gather all the images, and try to load them from the zip file.
				const JsonReader images( models.GetChildByName( "images" ) );
				if ( images.IsArray() )
				{
					while ( !images.IsEndOfArray() )
					{
						const JsonReader image( images.GetNextArrayElement() );
						if ( image.IsObject() )
						{
							const String name = image.GetChildStringByName( "name" );
							const String uri = image.GetChildStringByName( "uri" );
							int bufferView = image.GetChildInt32ByName( "bufferView", -1 );
							if ( bufferView >= 0 )
							{
								// #TODO: support bufferView index for image files.
								WARN( "Loading images from bufferView currently unsupported, defaulting image" );
								// Create a default texture.
								LoadModelFileTexture( modelFile, "DefaultImage", nullptr, 0, materialParms );
							}
							else
							{
								// check to make sure the image is ktx.
								if ( OVR_stricmp( uri.GetExtension().ToCStr(), ".ktx" ) != 0 )
								{
									// #TODO: Try looking for a ktx image before we load the non ktx image.
									WARN( "Loading images other then ktx is not advised. %s", uri.ToCStr() );

									int bufferLength = 0;
									uint8_t * buffer = ReadFileBufferFromZipFile( zfp, uri.ToCStr(), bufferLength, ( const uint8_t * )fileData );
									const char * imageName = uri.ToCStr();

									LoadModelFileTexture( modelFile, imageName, ( const char * )buffer, bufferLength, materialParms );
								}
								else
								{
									int bufferLength = 0;
									uint8_t * buffer = ReadFileBufferFromZipFile( zfp, uri.ToCStr(), bufferLength, ( const uint8_t * )fileData );
									const char * imageName = uri.ToCStr();

									LoadModelFileTexture( modelFile, imageName, ( const char * )buffer, bufferLength, materialParms );
								}
							}
						}
					}
				}
			} // END images
			// End of section dependent on zip file.
		}
		else
		{
			WARN( "error: could not parse json for gltf" );
			loaded = false;
		}
		json->Release();

		if ( loaded )
		{
			loaded = LoadModelFile_glTF_Json( modelFile, gltfJson, programs, materialParms, outModelGeo );
		}
	}

	if ( gltfJson != nullptr && ( gltfJson < fileData || gltfJson > fileData + fileDataLength ) )
	{
		delete gltfJson;
	}

	return loaded;
}

ModelFile * LoadModelFile_glB( const char * fileName,
	const char * fileData, const int fileDataLength,
	const ModelGlPrograms & programs,
	const MaterialParms & materialParms,
	ModelGeo * outModelGeo )
{
	LOGCPUTIME( "LoadModelFile_glB" );

	ModelFile * modelFilePtr = new ModelFile;
	ModelFile & modelFile = *modelFilePtr;

	modelFile.FileName = fileName;
	modelFile.UsingSrgbTextures = materialParms.UseSrgbTextureFormats;

	bool loaded = true;
	
	uint32_t fileDataIndex = 0;
	uint32_t fileDataRemainingLength = fileDataLength;
	glTFBinaryHeader header;
	if ( fileDataRemainingLength < sizeof( header ) )
	{
		WARN( "Error: could not load glb gltfHeader" );
		loaded = false;
	}

	if ( loaded )
	{
		memcpy( &header, &fileData[fileDataIndex], sizeof( header ) );
		fileDataIndex += sizeof( header );
		fileDataRemainingLength -= sizeof( header );

		if ( header.magic != GLTF_BINARY_MAGIC )
		{
			WARN( "Error: invalid glb gltfHeader magic" );
			loaded = false;
		}

		if ( header.version != GLTF_BINARY_VERSION )
		{
			WARN( "Error: invalid glb gltfHeader version" );
			loaded = false;
		}

		if ( header.length != ( uint32_t )fileDataLength )
		{
			WARN( "Error: invalid glb gltfHeader length" );
			loaded = false;
		}
	}

	if ( loaded && fileDataRemainingLength > sizeof( uint32_t ) * 2 )
	{
		uint32_t chunkType = 0;
		uint32_t chunkLength = 0;

		memcpy( &chunkLength, &fileData[fileDataIndex], sizeof( uint32_t ) );
		fileDataIndex += sizeof( uint32_t );
		fileDataRemainingLength -= sizeof( uint32_t );
		memcpy( &chunkType, &fileData[fileDataIndex], sizeof( uint32_t ) );
		fileDataIndex += sizeof( uint32_t );
		fileDataRemainingLength -= sizeof( uint32_t );

		if ( chunkType != GLTF_BINARY_CHUNKTYPE_JSON )
		{
			WARN( "Error: glb first chunk not JSON" );
			loaded = false;
		}

		JSON * json = nullptr;
		const char * gltfJson = nullptr;
		if ( loaded )
		{
			const char * error = nullptr;
			gltfJson = &fileData[fileDataIndex];
			json = JSON::Parse( gltfJson, &error );
			fileDataIndex += chunkLength;
			fileDataRemainingLength -= chunkLength;

			if ( json == nullptr )
			{
				WARN( "LoadModelFile_glB: Error Parsing JSON %s : %s", modelFilePtr->FileName.ToCStr(), error );
				loaded = false;
			}
		}

		const char * buffer = nullptr;
		uint32_t bufferLength = 0;
		if ( loaded )
		{
			if ( fileDataRemainingLength > sizeof( uint32_t ) * 2 )
			{
				uint32_t bufferChunkType = 0;
				memcpy( &bufferLength, &fileData[fileDataIndex], sizeof( uint32_t ) );
				fileDataIndex += sizeof( uint32_t );
				fileDataRemainingLength -= sizeof( uint32_t );
				memcpy( &bufferChunkType, &fileData[fileDataIndex], sizeof( uint32_t ) );
				fileDataIndex += sizeof( uint32_t );
				fileDataRemainingLength -= sizeof( uint32_t );

				if ( bufferChunkType != GLTF_BINARY_CHUNKTYPE_BINARY )
				{
					WARN( "Error: glb second chunk not binary" );
					loaded = false;
				}
				else if ( bufferLength > fileDataRemainingLength )
				{
					WARN( "Error: glb binary chunk length greater then remaining buffer" );
					loaded = false;
				}
				else
				{
					if ( bufferLength < fileDataRemainingLength )
					{
						WARN( "Error: glb binary chunk length less then remaining buffer" );
					}
					buffer = &fileData[fileDataIndex];
				}
			}
			else
			{
				WARN( "Not enough data remaining to parse glB buffer" );
				loaded = false;
			}
		}

		if ( loaded )
		{
			const JsonReader models( json );
			if ( models.IsObject() )
			{
				// Buffers BufferViews and Images need access to the data location, in this case the buffer inside the glb file.   
				//	after they are loaded it should be identical wether the input is a zip file, a folder structure or a glb file.
				if ( loaded )
				{ // BUFFERS
					LOGV( "Loading buffers" );
					// gather all the buffers, and try to load them from the zip file.
					const JsonReader buffers( models.GetChildByName( "buffers" ) );
					if ( buffers.IsArray() )
					{
						while ( !buffers.IsEndOfArray() && loaded )
						{
							if ( modelFile.Buffers.GetSizeI() > 0 )
							{
								WARN( "Error: glB file contains more then one buffer" );
								loaded = false;
							}

							const JsonReader bufferReader( buffers.GetNextArrayElement() );
							if ( bufferReader.IsObject() && loaded )
							{
								ModelBuffer newGltfBuffer;

								const String name = bufferReader.GetChildStringByName( "name" );
								const String uri = bufferReader.GetChildStringByName( "uri" );
								newGltfBuffer.byteLength = bufferReader.GetChildInt32ByName( "byteLength", -1 );

								//  #TODO: proper uri reading.  right now, assuming its a file name.
								if ( !uri.IsEmpty() )
								{
									WARN( "Loading buffers with an uri currently unsupported in glb" );
									loaded = false;
								}

								if ( newGltfBuffer.byteLength > ( size_t )bufferLength )
								{
									WARN( "%d byteLength > bufferLength loading gltfBuffer %d", ( int )newGltfBuffer.byteLength, bufferLength );
									loaded = false;
								}

								// ensure the buffer is aligned.
								newGltfBuffer.bufferData = ( uint8_t * )( new float[newGltfBuffer.byteLength / 4 + 1] );
								memcpy( newGltfBuffer.bufferData, buffer, newGltfBuffer.byteLength );
								newGltfBuffer.bufferData[newGltfBuffer.byteLength] = '\0';

								const char * bufferName;
								if ( name.GetLength() > 0 )
								{
									bufferName = name.ToCStr();
								}
								else
								{
									bufferName = "glB_Buffer";
								}

								newGltfBuffer.name = bufferName;

								modelFile.Buffers.PushBack( newGltfBuffer );
							}
						}
					}
				} // END BUFFERS

				if ( loaded )
				{ // BUFFERVIEW
					LOGV( "Loading bufferviews" );
					const JsonReader bufferViews( models.GetChildByName( "bufferViews" ) );
					if ( bufferViews.IsArray() )
					{
						while ( !bufferViews.IsEndOfArray() && loaded )
						{
							const JsonReader bufferview( bufferViews.GetNextArrayElement() );
							if ( bufferview.IsObject() )
							{
								ModelBufferView newBufferView;

								newBufferView.name = bufferview.GetChildStringByName( "name" );
								const int bufferIndex = bufferview.GetChildInt32ByName( "buffer" );
								newBufferView.byteOffset = bufferview.GetChildInt32ByName( "byteOffset" );
								newBufferView.byteLength = bufferview.GetChildInt32ByName( "byteLength" );
								newBufferView.byteStride = bufferview.GetChildInt32ByName( "byteStride" );
								newBufferView.target = bufferview.GetChildInt32ByName( "target" );

								if ( bufferIndex < 0 || bufferIndex >= ( const int )modelFile.Buffers.GetSize() )
								{
									WARN( "Error: Invalid buffer Index in gltfBufferView" );
									loaded = false;
								}
								if ( newBufferView.byteStride < 0 || newBufferView.byteStride > 255 )
								{
									WARN( "Error: Invalid byeStride in gltfBufferView" );
									loaded = false;
								}
								if ( newBufferView.target < 0 )
								{
									WARN( "Error: Invalid target in gltfBufferView" );
									loaded = false;
								}

								newBufferView.buffer = &modelFile.Buffers[bufferIndex];
								modelFile.BufferViews.PushBack( newBufferView );
							}
						}
					}
				} // END BUFFERVIEWS

				if ( loaded )
				{ // IMAGES
					LOGV( "Loading image textures" );
					// gather all the images, and try to load them from the zip file.
					const JsonReader images( models.GetChildByName( "images" ) );
					if ( images.IsArray() )
					{
						while ( !images.IsEndOfArray() )
						{
							const JsonReader image( images.GetNextArrayElement() );
							if ( image.IsObject() )
							{
								const String name = image.GetChildStringByName( "name" );
								const String uri = image.GetChildStringByName( "uri" );
								int bufferView = image.GetChildInt32ByName( "bufferView", -1 );
								LOGV( "LoadModelFile_glB: %s, %s, %d", name.ToCStr(), uri.ToCStr(), bufferView );
								if ( bufferView >= 0 && bufferView < modelFile.BufferViews.GetSizeI() )
								{
									ModelBufferView * pBufferView = &modelFile.BufferViews[bufferView];
									int imageBufferLength = (int)pBufferView->byteLength;
									uint8_t * imageBuffer = ( pBufferView->buffer->bufferData + pBufferView->byteOffset );
									
									// passing in .png since currently these are only ever jpg or png, and that is the same path for our texture loader.
									// #TODO: get this supporting ktx.  Our loader should be looking at the buffer not the name for determining format.
									LoadModelFileTexture( modelFile, "DefualtImage.png", ( const char * )imageBuffer, imageBufferLength, materialParms );
									
								}
								else
								{
									WARN( "Loading images from othen then bufferView currently unsupported in glBfd, defaulting image" );
									// Create a default texture.
									LoadModelFileTexture( modelFile, "DefaultImage", nullptr, 0, materialParms );
								}
							}
						}
					}
				} // END images


				  // End of section dependent on buffer data in the glB file.
			}
		}

		if ( json != nullptr )
		{
			json->Release();
		}

		if ( loaded )
		{
			loaded = LoadModelFile_glTF_Json( modelFile, gltfJson, programs, materialParms, outModelGeo );
		}
	}

	//delete fileData;

	if ( !loaded )
	{
		WARN( "Error: failed to load %s", fileName );
		delete modelFilePtr;
		modelFilePtr = nullptr;
	}

	return modelFilePtr;
}

} // namespace OVR
