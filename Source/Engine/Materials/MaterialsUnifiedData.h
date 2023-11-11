#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"


namespace spt::mat
{

constexpr rhi::SamplerDefinition CreateMaterialTexturesSampler()
{
	rhi::SamplerDefinition sampler = rhi::SamplerState::LinearRepeat;
	sampler.mipLodBias = -0.5f;
	return sampler;
}


DS_BEGIN(MaterialsDS, rg::RGDescriptorSetState<MaterialsDS>)
	DS_BINDING(BINDING_TYPE(gfx::ByteAddressBuffer),                                        u_materialsData)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<1024>),                        u_materialsTextures)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<CreateMaterialTexturesSampler()>), u_materialTexturesSampler)
DS_END();


class MATERIALS_API MaterialsUnifiedData
{
public:

	static MaterialsUnifiedData& Get();

	Uint32 AddMaterialTexture(const lib::SharedRef<rdr::TextureView>& texture);

	rhi::RHISuballocation CreateMaterialDataSuballocation(Uint32 dataSize);
	rhi::RHISuballocation CreateMaterialDataSuballocation(const Byte* materialData, Uint32 dataSize);

	lib::MTHandle<MaterialsDS> GetMaterialsDS() const;

private:

	MaterialsUnifiedData();

	lib::SharedRef<rdr::Buffer> CreateMaterialsUnifiedBuffer() const;
	lib::MTHandle<MaterialsDS> CreateMaterialsDS() const;

	lib::SharedPtr<rdr::Buffer> m_materialsUnifiedBuffer;

	lib::MTHandle<MaterialsDS> m_materialsDS;
};

} // spt::mat