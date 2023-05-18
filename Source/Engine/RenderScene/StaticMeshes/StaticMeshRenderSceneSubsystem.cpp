#include "StaticMeshRenderSceneSubsystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"
#include "StaticMeshGeometry.h"
#include "Materials/MaterialTypes.h"
#include "View/RenderView.h"
#include "Lights/LightTypes.h"

namespace spt::rsc
{

StaticMeshRenderSceneSubsystem::StaticMeshRenderSceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
{ }

StaticMeshBatchDefinition StaticMeshRenderSceneSubsystem::BuildBatchForView(const RenderView& view, EMaterialType materialType) const
{
	SPT_PROFILER_FUNCTION();

	StaticMeshBatchDefinition batch;

	const auto meshesView = GetOwningScene().GetRegistry().view<TransformComponent, StaticMeshInstanceRenderData, EntityGPUDataHandle, MaterialsDataComponent>();
	for (const auto& [entity, transformComp, staticMeshRenderData, entityGPUDataHandle, materialsData] : meshesView.each())
	{
		const RenderingDataEntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		const math::Vector3f boundingSphereCenterWS = transformComp.GetTransform() * meshRenderingDef.boundingSphereCenter;
		const Real32 boundingSphereRadius = meshRenderingDef.boundingSphereRadius * transformComp.GetUniformScale();

		if (view.IsSphereOverlappingFrustum(boundingSphereCenterWS, boundingSphereRadius))
		{
			const Uint32 entityIdx = entityGPUDataHandle.GetEntityIdx();
			AppendMeshToBatch(batch, entityIdx, staticMeshRenderData, meshRenderingDef, materialsData, materialType);
		}
	}

	return batch;
}

StaticMeshBatchDefinition StaticMeshRenderSceneSubsystem::BuildBatchForPointLight(const PointLightData& pointLight, EMaterialType materialType) const
{
	SPT_PROFILER_FUNCTION();

	StaticMeshBatchDefinition batch;

	const auto meshesView = GetOwningScene().GetRegistry().view<TransformComponent, StaticMeshInstanceRenderData, EntityGPUDataHandle, MaterialsDataComponent>();
	for (const auto& [entity, transformComp, staticMeshRenderData, entityGPUDataHandle, materialsData] : meshesView.each())
	{
		const RenderingDataEntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		const math::Vector3f boundingSphereCenterWS = transformComp.GetTransform() * meshRenderingDef.boundingSphereCenter;
		const Real32 boundingSphereRadius = meshRenderingDef.boundingSphereRadius * transformComp.GetUniformScale();

		if ((boundingSphereCenterWS - pointLight.location).squaredNorm() < math::Utils::Square(pointLight.radius + boundingSphereRadius))
		{
			const Uint32 entityIdx = entityGPUDataHandle.GetEntityIdx();
			AppendMeshToBatch(batch, entityIdx, staticMeshRenderData, meshRenderingDef, materialsData, materialType);
		}
	}

	return batch;
}

void StaticMeshRenderSceneSubsystem::AppendMeshToBatch(StaticMeshBatchDefinition& batch, Uint32 entityIdx, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef, const MaterialsDataComponent& materialsData, EMaterialType batchMaterialType) const
{
	for (Uint32 idx = 0; idx < meshRenderingDef.submeshesNum; ++idx)
	{
		const RenderingDataEntityHandle material = materialsData.materials[idx];
		const MaterialCommonData& materialData = material.get<MaterialCommonData>();

		if(materialData.materialType == batchMaterialType)
		{
			const Uint32 globalSubmeshIdx = meshRenderingDef.submeshesBeginIdx + idx;

			StaticMeshBatchElement batchElement;
			batchElement.entityIdx			= entityIdx;
			batchElement.submeshGlobalIdx	= globalSubmeshIdx;
			batchElement.materialDataOffset	= static_cast<Uint32>(materialData.materialDataSuballocation.GetOffset());
			batch.batchElements.emplace_back(batchElement);
		}
	}

	batch.maxMeshletsNum	+= meshRenderingDef.maxMeshletsNum;
	batch.maxTrianglesNum	+= meshRenderingDef.maxTrianglesNum;
}

} // spt::rsc
