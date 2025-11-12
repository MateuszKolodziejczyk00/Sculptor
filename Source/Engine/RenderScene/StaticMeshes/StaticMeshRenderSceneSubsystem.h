#pragma once

#include "RenderSceneSubsystem.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RenderSceneRegistry.h"
#include "ECSRegistry.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Geometry/GeometryTypes.h"
#include "Materials/MaterialsRenderingCommon.h"


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
SPT_REGISTER_COMPONENT_TYPE(StaticMeshInstanceRenderData, RenderSceneRegistry);


BEGIN_SHADER_STRUCT(StaticMeshBatchElement)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr, entityPtr)
	SHADER_STRUCT_FIELD(SubmeshGPUPtr,      submeshPtr)
	SHADER_STRUCT_FIELD(Uint16,             materialDataID)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMGPUBatchData)
	SHADER_STRUCT_FIELD(Uint32, elementsNum)
END_SHADER_STRUCT();


DS_BEGIN(StaticMeshBatchDS, rg::RGDescriptorSetState<StaticMeshBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>), u_batchElements)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SMGPUBatchData>),           u_batchData)
DS_END();


BEGIN_SHADER_STRUCT(SMDepthOnlyPermutation)
	SHADER_STRUCT_FIELD(mat::MaterialShader, SHADER)
	SHADER_STRUCT_FIELD(Bool,                CUSTOM_OPACITY)
END_SHADER_STRUCT();


struct StaticMeshSMBatchDefinition
{
	StaticMeshSMBatchDefinition()
		: batchElementsNum(0)
		, maxMeshletsNum(0)
		, maxTrianglesNum(0)
	{ }

	Uint32 batchElementsNum;
	Uint32 maxMeshletsNum;
	Uint32 maxTrianglesNum;

	SMDepthOnlyPermutation permutation;

	lib::MTHandle<StaticMeshBatchDS> batchDS;
};


class SMBatchesBuilder
{
public:

	SMBatchesBuilder(lib::DynamicArray<StaticMeshSMBatchDefinition>& inBatches);

	void AppendMeshToBatch(RenderEntityGPUPtr entityPtr, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef, const rsc::MaterialSlotsComponent& materialsSlots);

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
		SMDepthOnlyPermutation permutation;
	};


	BatchBuildData& GetBatchBuildDataForMaterial(const SMDepthOnlyPermutation& permutation);

	StaticMeshSMBatchDefinition FinalizeBatchDefinition(const BatchBuildData& batchBuildData) const;

	using PermutationsMap = lib::HashMap<SMDepthOnlyPermutation, Uint32, rdr::ShaderStructHasher<SMDepthOnlyPermutation>>;

	PermutationsMap m_permutationToBatchIdx;

	lib::DynamicArray<BatchBuildData>             m_batchBuildDatas;
	lib::DynamicArray<StaticMeshSMBatchDefinition>& m_batches;
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

	const lib::DynamicArray<StaticMeshSMBatchDefinition>& BuildBatchesForSMView(const RenderView& view) const;
	lib::DynamicArray<StaticMeshSMBatchDefinition>        BuildBatchesForPointLightSM(const PointLightData& pointLight) const;

	const GeometryPassDataCollection& GetCachedOpaqueGeometryPassData() const;
	const GeometryPassDataCollection& GetCachedTransparentGeometryPassData() const;

private:

	struct CachedSMBatches
	{
		lib::DynamicArray<StaticMeshSMBatchDefinition> batches;

		GeometryPassDataCollection opaqueGeometryPassData;
		GeometryPassDataCollection transparentGeometryPassData;
	};

	CachedSMBatches CacheStaticMeshBatches() const;

	CachedSMBatches m_cachedSMBatches;
};

} // spt::rsc
