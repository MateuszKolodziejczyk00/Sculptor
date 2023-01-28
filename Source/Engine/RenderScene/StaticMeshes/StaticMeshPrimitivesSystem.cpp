#include "StaticMeshPrimitivesSystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"
#include "StaticMeshGeometry.h"

namespace spt::rsc
{

StaticMeshPrimitivesSystem::StaticMeshPrimitivesSystem(RenderScene& owningScene)
	: Super(owningScene)
{ }

StaticMeshBatchDefinition StaticMeshPrimitivesSystem::BuildBatchForView(const RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

	StaticMeshBatchDefinition batch;

	const auto meshesView = GetOwningScene().GetRegistry().view<StaticMeshInstanceRenderData, EntityGPUDataHandle>();
	for (const auto& [entity, staticMeshRenderData, entityGPUDataHandle] : meshesView.each())
	{
		const RenderingDataEntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		for (Uint32 idx = 0; idx < meshRenderingDef.submeshesNum; ++idx)
		{
			const Uint32 globalSubmeshIdx = meshRenderingDef.submeshesBeginIdx + idx;

			StaticMeshBatchElement batchElement;
			batchElement.entityIdx = static_cast<Uint32>(entityGPUDataHandle.GetEntityIdx());
			batchElement.submeshGlobalIdx = globalSubmeshIdx;
			batch.batchElements.emplace_back(batchElement);
		}

		batch.maxMeshletsNum	+= meshRenderingDef.maxMeshletsNum;
		batch.maxTrianglesNum	+= meshRenderingDef.maxTrianglesNum;
	}

	return batch;
}

} // spt::rsc
