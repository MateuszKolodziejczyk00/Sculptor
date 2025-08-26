#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Material.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(GeometryBatchElement)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshGlobalIdx)
	SHADER_STRUCT_FIELD(Uint16, materialDataID)
	SHADER_STRUCT_FIELD(Uint16, materialBatchIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GPUVisibleMeshlet)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, meshletGlobalIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshGlobalIdx)
	SHADER_STRUCT_FIELD(Uint16, materialDataID)
	SHADER_STRUCT_FIELD(Uint16, materialBatchIdx)
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

	enum class EGenericType : Uint16
	{
		Opaque,
		Num
	};

	GeometryBatchShader()
	{ }

	GeometryBatchShader(EGenericType shader)
		: m_shader(shader)
	{ }

	explicit GeometryBatchShader(mat::MaterialShadersHash shader)
		: m_shader(shader)
	{ }

	void SetGenericShader(EGenericType shader)
	{
		m_shader = shader;
	}

	void SetCustomShader(mat::MaterialShadersHash shader)
	{
		m_shader = shader;
	}

	Bool IsGenericShader() const
	{
		return std::holds_alternative<EGenericType>(m_shader);
	}

	Bool IsCustomShader() const
	{
		return std::holds_alternative<mat::MaterialShadersHash>(m_shader);
	}

	EGenericType GetGenericShader() const
	{
		return std::get<EGenericType>(m_shader);
	}

	mat::MaterialShadersHash GetCustomShader() const
	{
		return std::get<mat::MaterialShadersHash>(m_shader);
	}

	Bool operator<=>(const GeometryBatchShader& other) const = default;

private:

	std::variant<EGenericType, mat::MaterialShadersHash> m_shader;
};


struct GeometryBatchPSOInfo
{
	struct Hasher
	{
		SizeType operator()(const GeometryBatchPSOInfo& psoInfo) const
		{
			if (psoInfo.shader.IsCustomShader())
			{
				return lib::GetHash(psoInfo.shader.GetCustomShader());
			}
			else
			{
				return lib::HashCombine(static_cast<Uint64>(psoInfo.shader.GetGenericShader()), psoInfo.isDoubleSided);
			}
		}

	};

	GeometryBatchPSOInfo()
		: isDoubleSided(false)
	{
	}

	Bool operator<=>(const GeometryBatchPSOInfo& other) const = default;

	GeometryBatchShader shader;
	Bool isDoubleSided : 1;
};


struct GeometryBatch
{
	Uint32 batchElementsNum = 0u;
	Uint32 batchMeshletsNum = 0u;
	GeometryBatchPSOInfo psoInfo;

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
