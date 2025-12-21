#include "StaticMeshRenderSceneSubsystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"
#include "StaticMeshGeometry.h"
#include "View/RenderView.h"
#include "Lights/LightTypes.h"
#include "Transfers/UploadUtils.h"
#include "Geometry/GeometryDrawer.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// SMBatchesBuilder ==============================================================================

SMBatchesBuilder::SMBatchesBuilder(lib::DynamicArray<StaticMeshSMBatchDefinition>& inBatches)
	: m_batches(inBatches)
{ }

void SMBatchesBuilder::AppendMeshToBatch(RenderEntityGPUPtr entityPtr, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef, const rsc::MaterialSlotsComponent& materialsSlots)
{
	for (Uint32 idx = 0; idx < meshRenderingDef.submeshesDefs.size(); ++idx)
	{
		const ecs::EntityHandle material = materialsSlots.slots[idx];
		const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

		if (materialProxy.params.transparent)
		{
			continue;
		}

		const SubmeshRenderingDefinition& submeshDef = meshRenderingDef.submeshesDefs[idx];

		SMDepthOnlyPermutation permutation;

		if (materialProxy.params.customOpacity) // No material evaluation needed if custom opacity is not used
		{
			permutation.SHADER         = materialProxy.params.shader;
			permutation.CUSTOM_OPACITY = materialProxy.params.customOpacity;
		}

		BatchBuildData& batch = GetBatchBuildDataForMaterial(permutation);

		const SubmeshGPUPtr submeshPtr = meshRenderingDef.submeshesPtr + idx;

		StaticMeshBatchElement batchElement;
		batchElement.entityPtr          = entityPtr;
		batchElement.submeshPtr         = submeshPtr;
		batchElement.materialDataHandle = materialProxy.GetMaterialDataHandle();
		batch.batchElements.emplace_back(batchElement);

		batch.maxMeshletsNum  += submeshDef.meshletsNum;
		batch.maxTrianglesNum += submeshDef.trianglesNum;
	}
}

void SMBatchesBuilder::FinalizeBatches()
{
	SPT_PROFILER_FUNCTION();

	m_batches.reserve(m_batches.size() + m_batchBuildDatas.size());
	for (const BatchBuildData& batchBuildData : m_batchBuildDatas)
	{
		if (batchBuildData.IsValid())
		{
			m_batches.emplace_back(FinalizeBatchDefinition(batchBuildData));
		}
	}
}

SMBatchesBuilder::BatchBuildData& SMBatchesBuilder::GetBatchBuildDataForMaterial(const SMDepthOnlyPermutation& permutation)
{
	const auto it = m_permutationToBatchIdx.find(permutation);
	if (it != m_permutationToBatchIdx.end())
	{
		return m_batchBuildDatas[it->second];
	}
	else
	{
		const Uint32 batchIdx = static_cast<Uint32>(m_batchBuildDatas.size());
		m_permutationToBatchIdx[permutation] = batchIdx;

		BatchBuildData& batchData = m_batchBuildDatas.emplace_back();
		batchData.permutation = permutation;

		return batchData;
	}
}

StaticMeshSMBatchDefinition SMBatchesBuilder::FinalizeBatchDefinition(const BatchBuildData& batchBuildData) const
{
	StaticMeshSMBatchDefinition batchDef;

	batchDef.batchElementsNum = static_cast<Uint32>(batchBuildData.batchElements.size());
	batchDef.maxMeshletsNum   = batchBuildData.maxMeshletsNum;
	batchDef.maxTrianglesNum  = batchBuildData.maxTrianglesNum;
	batchDef.permutation      = batchBuildData.permutation;

	SMGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batchBuildData.batchElements.size());

	const Uint64 batchDataSize = sizeof(StaticMeshBatchElement) * batchBuildData.batchElements.size();
	const rhi::BufferDefinition batchBufferDef(batchDataSize, rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> batchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StaticMeshesBatch"), batchBufferDef, rhi::EMemoryUsage::CPUToGPU);

	gfx::UploadDataToBuffer(batchBuffer, 0, reinterpret_cast<const Byte*>(batchBuildData.batchElements.data()), batchDataSize);

	const lib::MTHandle<StaticMeshBatchDS> batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("StaticMeshesBatchDS"));
	batchDS->u_batchElements = batchBuffer->GetFullView();
	batchDS->u_batchData     = gpuBatchData;

	batchDef.batchDS = batchDS;

	return batchDef;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// StaticMeshRenderSceneSubsystem ================================================================

StaticMeshRenderSceneSubsystem::StaticMeshRenderSceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
{ }

void StaticMeshRenderSceneSubsystem::Update()
{
	SPT_PROFILER_FUNCTION();

	Super::Update();

	m_cachedSMBatches = CacheStaticMeshBatches();
}

const lib::DynamicArray<StaticMeshSMBatchDefinition>& StaticMeshRenderSceneSubsystem::BuildBatchesForSMView(const RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

	return m_cachedSMBatches.batches;
}

lib::DynamicArray<StaticMeshSMBatchDefinition> StaticMeshRenderSceneSubsystem::BuildBatchesForPointLightSM(const PointLightData& pointLight) const
{
	SPT_PROFILER_FUNCTION();
	
	lib::DynamicArray<StaticMeshSMBatchDefinition> batches;

	SMBatchesBuilder batchesBuilder(batches);

	const auto meshesView = GetOwningScene().GetRegistry().view<TransformComponent, StaticMeshInstanceRenderData, EntityGPUDataHandle, rsc::MaterialSlotsComponent>();
	for (const auto& [entity, transformComp, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const ecs::EntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<StaticMeshRenderingDefinition>();

		const math::Vector3f boundingSphereCenterWS = transformComp.GetTransform() * meshRenderingDef.boundingSphereCenter;
		const Real32 boundingSphereRadius = meshRenderingDef.boundingSphereRadius * transformComp.GetUniformScale();

		if ((boundingSphereCenterWS - pointLight.location).squaredNorm() < math::Utils::Square(pointLight.radius + boundingSphereRadius))
		{
			batchesBuilder.AppendMeshToBatch(entityGPUDataHandle.GetGPUDataPtr(), staticMeshRenderData, meshRenderingDef, materialsSlots);
		}
	}

	batchesBuilder.FinalizeBatches();

	return batches;
}

const GeometryPassDataCollection& StaticMeshRenderSceneSubsystem::GetCachedOpaqueGeometryPassData() const
{
	return m_cachedSMBatches.opaqueGeometryPassData;
}

const GeometryPassDataCollection& StaticMeshRenderSceneSubsystem::GetCachedTransparentGeometryPassData() const
{
	return m_cachedSMBatches.transparentGeometryPassData;
}

StaticMeshRenderSceneSubsystem::CachedSMBatches StaticMeshRenderSceneSubsystem::CacheStaticMeshBatches() const
{
	SPT_PROFILER_FUNCTION();

	CachedSMBatches cachedBatches;

	SMBatchesBuilder batchesBuilder(cachedBatches.batches);

	GeometryDrawer opaqueGeometryDrawer(OUT cachedBatches.opaqueGeometryPassData);
	GeometryDrawer transparentGeometryDrawer(OUT cachedBatches.transparentGeometryPassData);

	const auto meshesView = GetOwningScene().GetRegistry().view<const StaticMeshInstanceRenderData, const EntityGPUDataHandle, const rsc::MaterialSlotsComponent>();
	// Legacy (currently, shadows only)
	for (const auto& [entity, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const ecs::EntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<const StaticMeshRenderingDefinition>();

		const RenderEntityGPUPtr entityPtr = entityGPUDataHandle.GetGPUDataPtr();
		batchesBuilder.AppendMeshToBatch(entityPtr, staticMeshRenderData, meshRenderingDef, materialsSlots);
	}

	// New system
	for (const auto& [entity, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const ecs::EntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<const StaticMeshRenderingDefinition>();

		for (Uint32 idx = 0; idx < meshRenderingDef.submeshesDefs.size(); ++idx)
		{
			const ecs::EntityHandle material = materialsSlots.slots[idx];
			const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

			const SubmeshRenderingDefinition& submeshDef = meshRenderingDef.submeshesDefs[idx];

			GeometryDefinition geometryDef;
			geometryDef.entityPtr   = entityGPUDataHandle.GetGPUDataPtr();
			geometryDef.submeshPtr  = meshRenderingDef.submeshesPtr + idx;
			geometryDef.meshletsNum = submeshDef.meshletsNum;

			if (materialProxy.params.transparent)
			{
				transparentGeometryDrawer.Draw(geometryDef, materialProxy);
			}
			else
			{
				opaqueGeometryDrawer.Draw(geometryDef, materialProxy);
			}
		}
	}

	batchesBuilder.FinalizeBatches();

	opaqueGeometryDrawer.FinalizeDraws();
	transparentGeometryDrawer.FinalizeDraws();

	return cachedBatches;

}

} // spt::rsc
