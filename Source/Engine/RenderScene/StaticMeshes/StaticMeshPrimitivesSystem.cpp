#include "StaticMeshPrimitivesSystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"
#include "StaticMeshGeometry.h"

namespace spt::rsc
{

StaticMeshPrimitivesSystem::StaticMeshPrimitivesSystem(RenderScene& owningScene)
	: Super(owningScene)
{ }

StaticMeshBatch StaticMeshPrimitivesSystem::BuildBatchForView(const RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

	StaticMeshBatch batch;

	const auto meshesView = GetOwningScene().GetRegistry().view<StaticMeshInstanceRenderData, EntityGPUDataHandle>();
	for (const auto& [entity, staticMeshRenderData, entityGPUDataHandle] : meshesView.each())
	{
		const RenderingDataEntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		StaticMeshBatchElement batchElement;
		batchElement.entityIdx = static_cast<Uint32>(entityGPUDataHandle.GetEntityIdx());
		batchElement.staticMeshIdx = meshRenderingDef.staticMeshIdx;
		batch.batchElements.emplace_back(batchElement);

		batch.maxSubmeshesNum	+= meshRenderingDef.maxSubmeshesNum;
		batch.maxMeshletsNum	+= meshRenderingDef.maxMeshletsNum;
		batch.maxTrianglesNum	+= meshRenderingDef.maxTrianglesNum;
	}

	return batch;
}

} // spt::rsc
