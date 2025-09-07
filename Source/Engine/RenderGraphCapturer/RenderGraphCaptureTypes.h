#pragma once

#include "SculptorCoreTypes.h"
#include "RHI/RHICore/RHIPipelineTypes.h"
#include "Types/Texture.h"


namespace spt::rg::capture
{

struct CapturedPass;

struct CapturedTexture
{
	struct Version
	{
		CapturedTexture*             owningTexture = nullptr;
		Uint32                       versionIdx = 0u;
		lib::SharedPtr<rdr::Texture> texture;
		CapturedPass*                producingPass; // might be null in case of first version of external texture
	};

	lib::HashedString name;
	rhi::TextureDefinition definition;
	lib::DynamicArray<lib::UniquePtr<Version>> versions;
};


struct CapturedBuffer
{
	struct Version
	{
		CapturedBuffer*         owningBuffer = nullptr;
		Uint32                  versionIdx = 0u;
		lib::DynamicArray<Byte> bufferData;
		CapturedPass*           producingPass; // might be null in case of first version of external buffer
	};

	lib::HashedString name;
	lib::DynamicArray<lib::UniquePtr<Version>> versions;
};


struct CapturedTextureBinding
{
	const CapturedTexture::Version* textureVersion = nullptr;
	lib::SharedPtr<rdr::TextureView> textureView;
	Uint32 baseMipLevel   = 0u;
	Uint32 mipLevelsNum   = 1u;
	Uint32 baseArrayLayer = 0u;
	Uint32 arrayLayersNum = 1u;
	Bool writable = false;
};


struct CapturedBufferBinding
{
	const CapturedBuffer::Version* bufferVersion = nullptr;
	Uint64 offset = 0u;
	Uint64 size   = 0u; // 0 means full buffer

	// type info
	lib::HashedString structTypeName;
	Uint32            elementsNum = 1u;
};


class RGCaptureSourceContext;

struct RGCaptureSourceInfo
{
	lib::WeakPtr<RGCaptureSourceContext> sourceContext;
};


struct RGCapturedNullProperties
{

};

struct RGCapturedComputeProperties
{
	rhi::PipelineStatistics pipelineStatistics;
};


using PassExecutionProperties = std::variant<RGCapturedNullProperties, RGCapturedComputeProperties>;


struct CapturedPass
{
	lib::HashedString name;

	PassExecutionProperties execProps;

	lib::DynamicArray<CapturedTextureBinding> textures;
	lib::DynamicArray<CapturedBufferBinding>  buffers;
};



struct RGCapture
{
	RGCaptureSourceInfo captureSource;

	lib::String name;

	lib::DynamicArray<lib::UniquePtr<CapturedPass>> passes;

	lib::DynamicArray<lib::UniquePtr<CapturedTexture>> textures;
	lib::DynamicArray<lib::UniquePtr<CapturedBuffer>>  buffers;

	lib::HashMap<Uint32, CapturedTexture*> descriptorIdxToTexture;
	lib::HashMap<Uint32, CapturedBuffer*>  descriptorIdxToBuffer;
};

} // spt::rg::capture
