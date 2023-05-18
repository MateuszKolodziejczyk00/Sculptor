#pragma once

#include "RenderSceneSubsystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"
#include "RenderingDataRegistry.h"
#include "Materials/MaterialTypes.h"


namespace spt::rsc
{

class RenderView;
struct PointLightData;
struct StaticMeshRenderingDefinition;
struct MaterialsDataComponent;


struct StaticMeshInstanceRenderData
{
	RenderingDataEntityHandle staticMesh;
};


BEGIN_SHADER_STRUCT(StaticMeshBatchElement)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshGlobalIdx)
	SHADER_STRUCT_FIELD(Uint32, materialDataOffset)
END_SHADER_STRUCT();


struct StaticMeshBatchDefinition
{
	StaticMeshBatchDefinition()
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
};


class RENDER_SCENE_API StaticMeshRenderSceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit StaticMeshRenderSceneSubsystem(RenderScene& owningScene);

	StaticMeshBatchDefinition BuildBatchForView(const RenderView& view, EMaterialType materialType) const;
	StaticMeshBatchDefinition BuildBatchForPointLight(const PointLightData& pointLight, EMaterialType materialType) const;

private:

	void AppendMeshToBatch(StaticMeshBatchDefinition& batch, Uint32 entityIdx, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef, const MaterialsDataComponent& materialsData, EMaterialType batchMaterialType) const;
};

} // spt::rsc
