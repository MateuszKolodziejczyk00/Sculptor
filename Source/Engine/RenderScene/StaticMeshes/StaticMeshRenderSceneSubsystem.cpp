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

SMBatchesBuilder::SMBatchesBuilder(lib::DynamicArray<StaticMeshBatchDefinition>& inBatches)
	: m_batches(inBatches)
{ }

void SMBatchesBuilder::AppendMeshToBatch(Uint32 entityIdx, const StaticMeshInstanceRenderData& instanceRenderData, const StaticMeshRenderingDefinition& meshRenderingDef, const rsc::MaterialSlotsComponent& materialsSlots)
{
	for (Uint32 idx = 0; idx < meshRenderingDef.submeshesDefs.size(); ++idx)
	{
		const ecs::EntityHandle material = materialsSlots.slots[idx];
		const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

		const SubmeshRenderingDefinition& submeshDef = meshRenderingDef.submeshesDefs[idx];

		BatchBuildData& batch = GetBatchBuildDataForMaterial(materialProxy.materialShadersHash);

		const Uint32 globalSubmeshIdx = meshRenderingDef.submeshesBeginIdx + idx;

		StaticMeshBatchElement batchElement;
		batchElement.entityIdx        = entityIdx;
		batchElement.submeshGlobalIdx = globalSubmeshIdx;
		batchElement.materialDataID   = materialProxy.GetMaterialDataID();
		batch.batchElements.emplace_back(batchElement);

		batch.maxMeshletsNum	+= submeshDef.meshletsNum;
		batch.maxTrianglesNum	+= submeshDef.trianglesNum;
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

SMBatchesBuilder::BatchBuildData& SMBatchesBuilder::GetBatchBuildDataForMaterial(mat::MaterialShadersHash materialShaderHash)
{
	const auto it = m_materialShaderHashToBatchIdx.find(materialShaderHash);
	if (it != m_materialShaderHashToBatchIdx.end())
	{
		return m_batchBuildDatas[it->second];
	}
	else
	{
		const Uint32 batchIdx = static_cast<Uint32>(m_batchBuildDatas.size());
		m_materialShaderHashToBatchIdx[materialShaderHash] = batchIdx;

		BatchBuildData& batchData = m_batchBuildDatas.emplace_back();
		batchData.materialShadersHash = materialShaderHash;

		return batchData;
	}
}

StaticMeshBatchDefinition SMBatchesBuilder::FinalizeBatchDefinition(const BatchBuildData& batchBuildData) const
{
	StaticMeshBatchDefinition batchDef;

	batchDef.batchElementsNum    = static_cast<Uint32>(batchBuildData.batchElements.size());
	batchDef.maxMeshletsNum      = batchBuildData.maxMeshletsNum;
	batchDef.maxTrianglesNum     = batchBuildData.maxTrianglesNum;
	batchDef.materialShadersHash = batchBuildData.materialShadersHash;

	SMGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batchBuildData.batchElements.size());

	const Uint64 batchDataSize = sizeof(StaticMeshBatchElement) * batchBuildData.batchElements.size();
	const rhi::BufferDefinition batchBufferDef(batchDataSize, rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> batchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StaticMeshesBatch"), batchBufferDef, rhi::EMemoryUsage::CPUToGPU);

	gfx::UploadDataToBuffer(batchBuffer, 0, reinterpret_cast<const Byte*>(batchBuildData.batchElements.data()), batchDataSize);

	const lib::MTHandle<StaticMeshBatchDS> batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("StaticMeshesBatchDS"));
	batchDS->u_batchElements = batchBuffer->CreateFullView();
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

const lib::DynamicArray<StaticMeshBatchDefinition>& StaticMeshRenderSceneSubsystem::BuildBatchesForView(const RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

	return m_cachedSMBatches.batches;
}

lib::DynamicArray<StaticMeshBatchDefinition> StaticMeshRenderSceneSubsystem::BuildBatchesForPointLight(const PointLightData& pointLight) const
{
	SPT_PROFILER_FUNCTION();
	
	lib::DynamicArray<StaticMeshBatchDefinition> batches;

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
			const Uint32 entityIdx = entityGPUDataHandle.GetEntityIdx();
			batchesBuilder.AppendMeshToBatch(entityIdx, staticMeshRenderData, meshRenderingDef, materialsSlots);
		}
	}

	batchesBuilder.FinalizeBatches();

	return batches;
}

const GeometryPassDataCollection& StaticMeshRenderSceneSubsystem::GetCachedGeometryPassData() const
{
	return m_cachedSMBatches.geometryPassData;
}

StaticMeshRenderSceneSubsystem::CachedSMBatches StaticMeshRenderSceneSubsystem::CacheStaticMeshBatches() const
{
	SPT_PROFILER_FUNCTION();

	CachedSMBatches cachedBatches;

	SMBatchesBuilder batchesBuilder(cachedBatches.batches);

	GeometryDrawer geometryDrawer(OUT cachedBatches.geometryPassData);

	const auto meshesView = GetOwningScene().GetRegistry().view<const StaticMeshInstanceRenderData, const EntityGPUDataHandle, const rsc::MaterialSlotsComponent>();
	for (const auto& [entity, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const ecs::EntityHandle staticMeshDataHandle = staticMeshRenderData.staticMesh;

		const StaticMeshRenderingDefinition& meshRenderingDef = staticMeshDataHandle.get<const StaticMeshRenderingDefinition>();

		const Uint32 entityIdx = entityGPUDataHandle.GetEntityIdx();
		batchesBuilder.AppendMeshToBatch(entityIdx, staticMeshRenderData, meshRenderingDef, materialsSlots);
	}

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
			geometryDef.entityGPUIdx     = entityGPUDataHandle.GetEntityIdx();
			geometryDef.submeshGlobalIdx = meshRenderingDef.submeshesBeginIdx + idx;
			geometryDef.meshletsNum      = submeshDef.meshletsNum;

			geometryDrawer.Draw(geometryDef, materialProxy);
		}
	}

	batchesBuilder.FinalizeBatches();
	geometryDrawer.FinalizeDraws();

	return cachedBatches;

}

} // spt::rsc
