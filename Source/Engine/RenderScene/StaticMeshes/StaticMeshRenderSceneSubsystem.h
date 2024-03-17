#pragma once

#include "RenderSceneSubsystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"
#include "ECSRegistry.h"
#include "Material.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Geometry/GeometryTypes.h"


namespace spt::rsc
{

class RenderView;
struct PointLightData;
struct StaticMeshRenderingDefinition;
struct MaterialsDataComponent;
class StaticMeshBatchDS;


struct StaticMeshInstanceRenderData
{
	ecs::EntityHandle staticMesh;
};


BEGIN_SHADER_STRUCT(StaticMeshBatchElement)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshGlobalIdx)
	SHADER_STRUCT_FIELD(Uint32, materialDataOffset)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMGPUBatchData)
	SHADER_STRUCT_FIELD(Uint32, elementsNum)
END_SHADER_STRUCT();


DS_BEGIN(StaticMeshBatchDS, rg::RGDescriptorSetState<StaticMeshBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>), u_batchElements)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SMGPUBatchData>),           u_batchData)
DS_END();


struct StaticMeshBatchDefinition
{
	StaticMeshBatchDefinition()
		: batchElementsNum(0)
		, maxMeshletsNum(0)
		, maxTrianglesNum(0)
		, materialShadersHash(0)
	{ }

	Uint32 batchElementsNum;
	Uint32 maxMeshletsNum;
	Uint32 maxTrianglesNum;

	mat::MaterialShadersHash materialShadersHash;

	lib::MTHandle<StaticMeshBatchDS> batchDS;
};


class SMBatchesBuilder
{
public:

	SMBatchesBuilder(lib::DynamicArray<StaticMeshBatchDefinition>& inBatches);

	void AppendMeshToBatch(Uint32 entityIdx, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef, const mat::MaterialSlotsComponent& materialsSlots);

	void FinalizeBatches();

private:


	struct BatchBuildData
	{
		BatchBuildData()
			: maxMeshletsNum(0)
			, maxTrianglesNum(0)
		{ }

		Bool IsValid() const
		{
			return !batchElements.empty();
		}

		lib::DynamicArray<StaticMeshBatchElement> batchElements;
		Uint32 maxMeshletsNum;
		Uint32 maxTrianglesNum;
		mat::MaterialShadersHash materialShadersHash;
	};


	BatchBuildData& GetBatchBuildDataForMaterial(mat::MaterialShadersHash materialShaderHash);

	StaticMeshBatchDefinition FinalizeBatchDefinition(const BatchBuildData& batchBuildData) const;

	lib::HashMap<mat::MaterialShadersHash, Uint32> m_materialShaderHashToBatchIdx;

	lib::DynamicArray<BatchBuildData>             m_batchBuildDatas;
	lib::DynamicArray<StaticMeshBatchDefinition>& m_batches;
};


class RENDER_SCENE_API StaticMeshRenderSceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit StaticMeshRenderSceneSubsystem(RenderScene& owningScene);

	// Begin RenderSceneSubsystem overrides
	virtual void Update() override;
	// End RenderSceneSubsystem overrides

	const lib::DynamicArray<StaticMeshBatchDefinition>& BuildBatchesForView(const RenderView& view) const;
	lib::DynamicArray<StaticMeshBatchDefinition>        BuildBatchesForPointLight(const PointLightData& pointLight) const;

	const GeometryPassDataCollection& GetCachedGeometryPassData() const;

private:

	struct CachedSMBatches
	{
		lib::DynamicArray<StaticMeshBatchDefinition> batches;

		GeometryPassDataCollection geometryPassData;
	};

	CachedSMBatches CacheStaticMeshBatches() const;

	CachedSMBatches m_cachedSMBatches;
};

} // spt::rsc
