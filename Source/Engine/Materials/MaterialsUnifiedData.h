#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::mat
{

BEGIN_SHADER_STRUCT(MaterialUnifiedData)
	SHADER_STRUCT_FIELD(gfx::ByteBufferRef, materialsData)
END_SHADER_STRUCT();


class MATERIALS_API MaterialsUnifiedData
{
public:

	static MaterialsUnifiedData& Get();

	void AddMaterialTexture(const lib::SharedRef<rdr::TextureView>& texture);

	rhi::RHIVirtualAllocation CreateMaterialDataSuballocation(Uint64 dataSize);
	rhi::RHIVirtualAllocation CreateMaterialDataSuballocation(const Byte* materialData, Uint64 dataSize);

	MaterialUnifiedData GetMaterialUnifiedData() const;

private:

	MaterialsUnifiedData();

	lib::SharedRef<rdr::Buffer> CreateMaterialsUnifiedBuffer() const;

	lib::SharedPtr<rdr::Buffer> m_materialsUnifiedBuffer;

	lib::DynamicArray<lib::SharedPtr<rdr::TextureView>> m_materialTextures;
};

} // spt::mat