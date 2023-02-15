#include "StaticMeshPrimitivesSystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"
#include "StaticMeshGeometry.h"
#include "Materials/MaterialTypes.h"
#include "View/RenderView.h"
#include "Lights/LightTypes.h"

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
		const Real32 boundingSphereRadius = meshRenderingDef.boundingSphereRadius * transformComp.GetUniformScale();

		if (view.IsSphereOverlappingFrustum(boundingSphereCenterWS, boundingSphereRadius))
		{
			const Uint32 entityIdx = static_cast<Uint32>(entityGPUDataHandle.GetEntityIdx());
			AppendMeshToBatch(batch, entityIdx, staticMeshRenderData, meshRenderingDef);
		}
	}

	return batch;
}

StaticMeshBatchDefinition StaticMeshPrimitivesSystem::BuildBatchForPointLight(const PointLightData& pointLight) const
{
	SPT_PROFILER_FUNCTION();

	StaticMeshBatchDefinition batch;

	const auto meshesView = GetOwningScene().GetRegistry().view<TransformComponent, StaticMeshInstanceRenderData, EntityGPUDataHandle>();
	for (const auto& [entity, transformComp, staticMeshRenderData, entityGPUDataHandle] : meshesView.each())
	{
		const RenderingDataEntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		const math::Vector3f boundingSphereCenterWS = transformComp.GetTransform() * meshRenderingDef.boundingSphereCenter;
		const Real32 boundingSphereRadius = meshRenderingDef.boundingSphereRadius * transformComp.GetUniformScale();

		if ((boundingSphereCenterWS - pointLight.location).squaredNorm() < math::Utils::Square(pointLight.radius + boundingSphereRadius))
		{
			const Uint32 entityIdx = static_cast<Uint32>(entityGPUDataHandle.GetEntityIdx());
			AppendMeshToBatch(batch, entityIdx, staticMeshRenderData, meshRenderingDef);
		}
	}

	return batch;
}

void StaticMeshPrimitivesSystem::AppendMeshToBatch(StaticMeshBatchDefinition& batch, Uint32 entityIdx, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef) const
{
	for (Uint32 idx = 0; idx < meshRenderingDef.submeshesNum; ++idx)
	{
		const RenderingDataEntityHandle material = instanceRenderData.materials[idx];
		const MaterialCommonData& materialData = material.get<MaterialCommonData>();

		const Uint32 globalSubmeshIdx = meshRenderingDef.submeshesBeginIdx + idx;

		StaticMeshBatchElement batchElement;
		batchElement.entityIdx			= entityIdx;
		batchElement.submeshGlobalIdx	= globalSubmeshIdx;
		batchElement.materialDataOffset	= static_cast<Uint32>(materialData.materialDataSuballocation.GetOffset());
		batch.batchElements.emplace_back(batchElement);
	}

	batch.maxMeshletsNum	+= meshRenderingDef.maxMeshletsNum;
	batch.maxTrianglesNum	+= meshRenderingDef.maxTrianglesNum;
}

} // spt::rsc
