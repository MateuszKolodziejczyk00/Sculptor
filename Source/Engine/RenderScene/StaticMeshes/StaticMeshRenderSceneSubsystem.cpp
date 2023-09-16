#include "StaticMeshRenderSceneSubsystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"
#include "StaticMeshGeometry.h"
#include "View/RenderView.h"
#include "Lights/LightTypes.h"
#include "Transfers/UploadUtils.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// SMBatchesBuilder ==============================================================================

SMBatchesBuilder::SMBatchesBuilder(lib::DynamicArray<StaticMeshBatchDefinition>& inBatches)
	: batches(inBatches)
{ }

void SMBatchesBuilder::AppendMeshToBatch(Uint32 entityIdx, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef, const mat::MaterialSlotsComponent& materialsSlots)
{
	for (Uint32 idx = 0; idx < meshRenderingDef.submeshesDefs.size(); ++idx)
	{
		const ecs::EntityHandle material = materialsSlots.slots[idx];
		const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

		const SubmeshRenderingDefinition& submeshDef = meshRenderingDef.submeshesDefs[idx];

		StaticMeshBatchDefinition& batch = GetBatchForMaterial(materialProxy.materialShadersHash);

		const Uint32 globalSubmeshIdx = meshRenderingDef.submeshesBeginIdx + idx;

		StaticMeshBatchElement batchElement;
		batchElement.entityIdx			= entityIdx;
		batchElement.submeshGlobalIdx	= globalSubmeshIdx;
		batchElement.materialDataOffset	= static_cast<Uint32>(materialProxy.materialDataSuballocation.GetOffset());
		batch.batchElements.emplace_back(batchElement);

		batch.maxMeshletsNum	+= submeshDef.meshletsNum;
		batch.maxTrianglesNum	+= submeshDef.trianglesNum;
	}
}

void SMBatchesBuilder::FinalizeBatches()
{
	SPT_PROFILER_FUNCTION();

	for (StaticMeshBatchDefinition& batch : batches)
	{
		CreateBatchDescriptorSet(batch);
	}
}

StaticMeshBatchDefinition& SMBatchesBuilder::GetBatchForMaterial(mat::MaterialShadersHash materialShaderHash)
{
	const auto it = materialShaderHashToBatchIdx.find(materialShaderHash);
	if (it != materialShaderHashToBatchIdx.end())
	{
		return batches[it->second];
	}
	else
	{
		const Uint32 batchIdx = static_cast<Uint32>(batches.size());
		materialShaderHashToBatchIdx[materialShaderHash] = batchIdx;

		batches.emplace_back();
		batches[batchIdx].materialShadersHash = materialShaderHash;

		return batches[batchIdx];
	}
}

void SMBatchesBuilder::CreateBatchDescriptorSet(StaticMeshBatchDefinition& batch) const
{
	SMGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batch.batchElements.size());

	const Uint64 batchDataSize = sizeof(StaticMeshBatchElement) * batch.batchElements.size();
	const rhi::BufferDefinition batchBufferDef(batchDataSize, rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> batchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StaticMeshesBatch"), batchBufferDef, rhi::EMemoryUsage::CPUToGPU);

	gfx::UploadDataToBuffer(batchBuffer, 0, reinterpret_cast<const Byte*>(batch.batchElements.data()), batchDataSize);

	const lib::SharedRef<StaticMeshBatchDS> batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("StaticMeshesBatchDS"));
	batchDS->u_batchElements	= batchBuffer->CreateFullView();
	batchDS->u_batchData		= gpuBatchData;

	batch.batchDS = batchDS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// StaticMeshRenderSceneSubsystem ================================================================

StaticMeshRenderSceneSubsystem::StaticMeshRenderSceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
{ }

lib::DynamicArray<StaticMeshBatchDefinition> StaticMeshRenderSceneSubsystem::BuildBatchesForView(const RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<StaticMeshBatchDefinition> batches;

	SMBatchesBuilder batchesBuilder(batches);

	const auto meshesView = GetOwningScene().GetRegistry().view<TransformComponent, StaticMeshInstanceRenderData, EntityGPUDataHandle, mat::MaterialSlotsComponent>();
	for (const auto& [entity, transformComp, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const ecs::EntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		const math::Vector3f boundingSphereCenterWS = transformComp.GetTransform() * meshRenderingDef.boundingSphereCenter;
		const Real32 boundingSphereRadius = meshRenderingDef.boundingSphereRadius * transformComp.GetUniformScale();

		if (view.IsSphereOverlappingFrustum(boundingSphereCenterWS, boundingSphereRadius))
		{
			const Uint32 entityIdx = entityGPUDataHandle.GetEntityIdx();
			batchesBuilder.AppendMeshToBatch(entityIdx, staticMeshRenderData, meshRenderingDef, materialsSlots);
		}
	}

	batchesBuilder.FinalizeBatches();

	return batches;
}

lib::DynamicArray<StaticMeshBatchDefinition> StaticMeshRenderSceneSubsystem::BuildBatchesForPointLight(const PointLightData& pointLight) const
{
	SPT_PROFILER_FUNCTION();
	
	lib::DynamicArray<StaticMeshBatchDefinition> batches;

	SMBatchesBuilder batchesBuilder(batches);

	const auto meshesView = GetOwningScene().GetRegistry().view<TransformComponent, StaticMeshInstanceRenderData, EntityGPUDataHandle, mat::MaterialSlotsComponent>();
	for (const auto& [entity, transformComp, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const ecs::EntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		const math::Vector3f boundingSphereCenterWS = transformComp.GetTransform() * meshRenderingDef.boundingSphereCenter;
		const Real32 boundingSphereRadius = meshRenderingDef.boundingSphereRadius * transformComp.GetUniformScale();

		if ((boundingSphereCenterWS - pointLight.location).squaredNorm() < math::Utils::Square(pointLight.radius + boundingSphereRadius))
		{
			const Uint32 entityIdx = entityGPUDataHandle.GetEntityIdx();
			batchesBuilder.AppendMeshToBatch(entityIdx, staticMeshRenderData, meshRenderingDef, materialsSlots);
		}
	}

	batchesBuilder.FinalizeBatches();

	return batches;
}

} // spt::rsc
