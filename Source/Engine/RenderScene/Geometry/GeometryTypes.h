#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Material.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(GeometryBatchElement)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshGlobalIdx)
	SHADER_STRUCT_FIELD(Uint32, materialDataOffset)
	SHADER_STRUCT_FIELD(Uint16, materialBatchIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryGPUWorkloadID)
	SHADER_STRUCT_FIELD(Uint32, data1)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GPUVisibleMeshlet)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, meshletGlobalIdx)
	SHADER_STRUCT_FIELD(Uint32, materialDataOffset)
	SHADER_STRUCT_FIELD(Uint32, materialBatchIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryGPUBatchData)
	SHADER_STRUCT_FIELD(Uint32, elementsNum)
END_SHADER_STRUCT();


DS_BEGIN(GeometryBatchDS, rg::RGDescriptorSetState<GeometryBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GeometryBatchElement>), u_batchElements)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GeometryGPUBatchData>),   u_batchData)
DS_END();


struct GeometryBatchShader
{
public:

	enum class EDefault
	{
		Opaque,
		Num
	};

	static constexpr SizeType GetDefaultShadersNum()
	{
		return static_cast<SizeType>(EDefault::Num);
	}

	GeometryBatchShader() = default;

	GeometryBatchShader(EDefault shader)
		: m_shader(shader)
	{ }

	explicit GeometryBatchShader(mat::MaterialShadersHash shader)
		: m_shader(shader)
	{ }

	void SetDefaultShader(EDefault shader)
	{
		m_shader = shader;
	}

	void SetCustomShader(mat::MaterialShadersHash shader)
	{
		m_shader = shader;
	}

	Bool IsDefaultShader() const
	{
		return std::holds_alternative<EDefault>(m_shader);
	}

	Bool IsCustomShader() const
	{
		return std::holds_alternative<mat::MaterialShadersHash>(m_shader);
	}

	EDefault GetDefaultShader() const
	{
		return std::get<EDefault>(m_shader);
	}

	mat::MaterialShadersHash GetCustomShader() const
	{
		return std::get<mat::MaterialShadersHash>(m_shader);
	}

private:

	std::variant<EDefault, mat::MaterialShadersHash> m_shader;
};


struct GeometryBatch
{
	Uint32 batchElementsNum = 0u;
	GeometryBatchShader shader;

	lib::MTHandle<GeometryBatchDS> batchDS;
};


struct MaterialBatch
{
	mat::MaterialShadersHash materialShadersHash;
};


struct GeometryPassDataCollection
{
	lib::DynamicArray<GeometryBatch> geometryBatches;
	lib::DynamicArray<MaterialBatch> materialBatches;
};

} // spt::rsc
