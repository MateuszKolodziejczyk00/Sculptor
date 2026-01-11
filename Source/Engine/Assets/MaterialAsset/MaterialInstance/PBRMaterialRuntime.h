#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "Transfers/GPUDeferredCommandsQueueTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::as


namespace spt::as
{

struct DDCLoadedBin;

struct MaterialTextureMipDefinition
{
	Uint32 dataOffset = 0u;
	Uint32 dataSize   = 0u;
};


struct MaterialTextureDefinition
{
	static constexpr Uint32 s_maxMipLevelsNum = 14u;

	math::Vector2u resolution   = math::Vector2u::Zero();
	Uint32 mipLevelsNum         = 0u;
	rhi::EFragmentFormat format = rhi::EFragmentFormat::None;
	MaterialTextureMipDefinition mipLevels[s_maxMipLevelsNum];
};


struct PBRMaterialDataHeader
{
	math::Vector3f baseColorFactor = math::Vector3f::Ones();
	Real32 metallicFactor          = 0.f;
	Real32 roughnessFactor         = 0.f;
	math::Vector3f emissionFactor  = math::Vector3f::Zero();

	MaterialTextureDefinition baseColorTexture;
	MaterialTextureDefinition metallicRoughnessTexture;
	MaterialTextureDefinition roughnessTexture;
	MaterialTextureDefinition normalsTexture;
	MaterialTextureDefinition emissiveTexture;
};

class MaterialTextureUploadRequest : public gfx::GPUDeferredUploadRequest
{
public:

	MaterialTextureUploadRequest() = default;

	// Begin GPUDeferredUploadRequest overrides
	virtual void EnqueueUploads() override;
	// End GPUDeferredUploadRequest overrides
	
	lib::SharedPtr<rdr::TextureView> dstTextureView;
	const MaterialTextureDefinition* textureDef = nullptr;
	lib::MTHandle<DDCLoadedBin>      blob;
};


} // spt::as
