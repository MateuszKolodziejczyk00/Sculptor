#include "StaticMeshPrimitivesSystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"
#include "StaticMeshGeometry.h"
#include "Materials/MaterialTypes.h"
#include "View/RenderView.h"

namespace spt::rsc
{

StaticMeshPrimitivesSystem::StaticMeshPrimitivesSystem(RenderScene& owningScene)
	: Super(owningScene)
{ }

StaticMeshBatchDefinition StaticMeshPrimitivesSystem::BuildBatchForView(const RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

	StaticMeshBatchDefinition batch;

	const auto meshesView = GetOwningScene().GetRegistry().view<TransformComponent, StaticMeshInstanceRenderData, EntityGPUDataHandle>();
	for (const auto& [entity, transformComp, staticMeshRenderData, entityGPUDataHandle] : meshesView.each())
	{
		const RenderingDataEntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		const math::Vector3f boundingSphereCenterWS = transformComp.GetTransform() * meshRenderingDef.boundingSphereCenter;
		const Real32 radius = meshRenderingDef.boundingSphereRadius * transformComp.GetUniformScale();

		if (!view.IsSphereOverlappingFrustum(boundingSphereCenterWS, radius))
		{
			continue;
		}

		for (Uint32 idx = 0; idx < meshRenderingDef.submeshesNum; ++idx)
		{
			const RenderingDataEntityHandle material = staticMeshRenderData.materials[idx];
			const MaterialCommonData& materialData = material.get<MaterialCommonData>();

			const Uint32 globalSubmeshIdx = meshRenderingDef.submeshesBeginIdx + idx;

			StaticMeshBatchElement batchElement;
			batchElement.entityIdx			= static_cast<Uint32>(entityGPUDataHandle.GetEntityIdx());
			batchElement.submeshGlobalIdx	= globalSubmeshIdx;
			batchElement.materialDataOffset	= static_cast<Uint32>(materialData.materialDataSuballocation.GetOffset());
			batch.batchElements.emplace_back(batchElement);
		}

		batch.maxMeshletsNum	+= meshRenderingDef.maxMeshletsNum;
		batch.maxTrianglesNum	+= meshRenderingDef.maxTrianglesNum;
	}

	return batch;
}

} // spt::rsc
