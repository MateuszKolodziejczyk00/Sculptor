#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"


namespace spt::rsc
{

DS_BEGIN(MaterialsDS, rg::RGDescriptorSetState<MaterialsDS>)
	DS_BINDING(BINDING_TYPE(gfx::ByteAddressBuffer),					u_materialsData)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<1024>),	u_materialsTextures)
DS_END();


class MaterialsUnifiedData
{
public:

	static MaterialsUnifiedData& Get();

	Uint32 AddMaterialTexture(const lib::SharedRef<rdr::TextureView>& texture);

	rhi::RHISuballocation CreateMaterialDataSuballocation(Uint32 dataSize);
	rhi::RHISuballocation CreateMaterialDataSuballocation(const Byte* materialData, Uint32 dataSize);

	lib::SharedRef<MaterialsDS> GetMaterialsDS() const;

private:

	MaterialsUnifiedData();

	lib::SharedRef<rdr::Buffer> CreateMaterialsUnifiedBuffer() const;
	lib::SharedRef<MaterialsDS> CreateMaterialsDS() const;

	lib::SharedPtr<rdr::Buffer> m_materialsUnifiedBuffer;

	lib::SharedPtr<MaterialsDS> m_materialsDS;
};

} // spt::rsc