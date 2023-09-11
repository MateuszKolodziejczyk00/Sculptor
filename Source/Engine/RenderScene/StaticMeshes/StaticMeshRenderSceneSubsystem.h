#pragma once

#include "RenderSceneSubsystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"
#include "ECSRegistry.h"
#include "Material.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


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
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_batchElements)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SMGPUBatchData>),	u_batchData)
DS_END();



struct StaticMeshBatchDefinition
{
	StaticMeshBatchDefinition()
		: maxMeshletsNum(0)
		, maxTrianglesNum(0)
		, materialShadersHash(0)
	{ }

	Bool IsValid() const
	{
		return !batchElements.empty();
	}

	lib::DynamicArray<StaticMeshBatchElement> batchElements;
	Uint32 maxMeshletsNum;
	Uint32 maxTrianglesNum;

	mat::MaterialShadersHash materialShadersHash;

	lib::SharedPtr<StaticMeshBatchDS> batchDS;
};


class SMBatchesBuilder
{
public:

	SMBatchesBuilder(lib::DynamicArray<StaticMeshBatchDefinition>& inBatches);

	void AppendMeshToBatch(Uint32 entityIdx, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef, const mat::MaterialSlotsComponent& materialsSlots, mat::EMaterialType batchMaterialType);

	void FinalizeBatches();

private:

	StaticMeshBatchDefinition& GetBatchForMaterial(mat::MaterialShadersHash materialShaderHash);

	void CreateBatchDescriptorSet(StaticMeshBatchDefinition& batch) const;

	lib::HashMap<mat::MaterialShadersHash, Uint32> materialShaderHashToBatchIdx;

	lib::DynamicArray<StaticMeshBatchDefinition>& batches;
};


class RENDER_SCENE_API StaticMeshRenderSceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit StaticMeshRenderSceneSubsystem(RenderScene& owningScene);

	lib::DynamicArray<StaticMeshBatchDefinition> BuildBatchesForView(const RenderView& view, mat::EMaterialType materialType) const;
	lib::DynamicArray<StaticMeshBatchDefinition> BuildBatchesForPointLight(const PointLightData& pointLight, mat::EMaterialType materialType) const;
};

} // spt::rsc
