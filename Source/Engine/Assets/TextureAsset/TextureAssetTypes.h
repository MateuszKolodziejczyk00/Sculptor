#pragma once

#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "RHI/RHICore/RHITextureTypes.h"
#include "DDC.h"
#include "Transfers/GPUDeferredUploadsQueueTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{


struct TextureDefinition
{
	math::Vector3u       resolution;
	rhi::EFragmentFormat format;
	Uint32               mipLevelsNum;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Resolution", resolution);
		serializer.Serialize("Format", format);
		serializer.Serialize("MipLevelsNum", mipLevelsNum);
	}
};


struct TextureSourceDefinition
{
	lib::Path path;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Path", path);
	}
};


struct CompiledMip
{
	Uint32 offset = 0u;
	Uint32 size   = 0u;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Offset", offset);
		serializer.Serialize("Size", size);
	}
};


struct CompiledTexture
{
	TextureDefinition definition;
	lib::DynamicArray<CompiledMip> mips;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Definition", definition);
		serializer.Serialize("Mips", mips);
	}
};


class TextureUploadRequest : public gfx::GPUDeferredUploadRequest
{
public:

	TextureUploadRequest() = default;

	// Begin GPUDeferredUploadRequest overrides
	virtual void PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency) override;
	virtual void EnqueueUploads() override;
	virtual void FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency) override;
	// End GPUDeferredUploadRequest overrides
	
	lib::SharedPtr<rdr::TextureView> dstTextureView;
	const CompiledTexture*           compiledTexture = nullptr;
	DerivedDataKey                   ddcKey;
	const AssetsSystem*              assetsSystem = nullptr;
};

} // spt::as
