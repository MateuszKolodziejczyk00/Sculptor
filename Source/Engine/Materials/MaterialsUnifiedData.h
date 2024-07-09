#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"


namespace spt::mat
{

constexpr rhi::SamplerDefinition CreateMaterialAnisoSampler()
{
	rhi::SamplerDefinition sampler = rhi::SamplerState::LinearRepeat;
	sampler.mipLodBias       = -0.5f;
	sampler.enableAnisotropy = true;
	sampler.maxAnisotropy    = 8.f;
	return sampler;
}


constexpr rhi::SamplerDefinition CreateMaterialLinearSampler()
{
	rhi::SamplerDefinition sampler = rhi::SamplerState::LinearRepeat;
	sampler.mipLodBias       = -0.5f;
	return sampler;
}


DS_BEGIN(MaterialsDS, rg::RGDescriptorSetState<MaterialsDS>)
	DS_BINDING(BINDING_TYPE(gfx::ByteAddressBuffer),                                      u_materialsData)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<1024>),                      u_materialsTextures)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<CreateMaterialAnisoSampler()>),  u_materialAnisoSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<CreateMaterialLinearSampler()>), u_materialLinearSampler)
DS_END();


class MATERIALS_API MaterialsUnifiedData
{
public:

	static MaterialsUnifiedData& Get();

	Uint32 AddMaterialTexture(const lib::SharedRef<rdr::TextureView>& texture);

	rhi::RHIVirtualAllocation CreateMaterialDataSuballocation(Uint64 dataSize);
	rhi::RHIVirtualAllocation CreateMaterialDataSuballocation(const Byte* materialData, Uint64 dataSize);

	lib::MTHandle<MaterialsDS> GetMaterialsDS() const;

private:

	MaterialsUnifiedData();

	lib::SharedRef<rdr::Buffer> CreateMaterialsUnifiedBuffer() const;
	lib::MTHandle<MaterialsDS> CreateMaterialsDS() const;

	lib::SharedPtr<rdr::Buffer> m_materialsUnifiedBuffer;

	lib::MTHandle<MaterialsDS> m_materialsDS;
};

} // spt::mat