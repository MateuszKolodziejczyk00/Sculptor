#include "GeometryDrawer.h"
#include "Utils/TransfersUtils.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// GeometryBatchesBuilder ========================================================================

GeometryBatchesBuilder::GeometryBatchesBuilder(GeometryPassDataCollection& batchesCollector)
	: m_batches(batchesCollector)
{
}

void GeometryBatchesBuilder::AppendGeometry(const GeometryDefinition& geometry, const mat::MaterialProxyComponent& materialProxy)
{
	SPT_CHECK(geometry.entityPtr.IsValid());
	SPT_CHECK(geometry.submeshPtr.IsValid());
	SPT_CHECK(geometry.meshletsNum > 0u);

	MaterialBatchPermutation materialBatchPermutation;
	materialBatchPermutation.SHADER       = materialProxy.params.shader;
	materialBatchPermutation.DOUBLE_SIDED = materialProxy.params.doubleSided;

	const Uint16 materialBatchIdx = GetMaterialBatchIdx(materialBatchPermutation);

	GeometryBatchElement newBatchElement;
	newBatchElement.entityPtr          = geometry.entityPtr;
	newBatchElement.submeshPtr         = geometry.submeshPtr;
	newBatchElement.materialDataHandle = materialProxy.GetMaterialDataHandle();
	newBatchElement.materialBatchIdx   = materialBatchIdx;

	GeometryBatchBuildData& geometryBatchData = GetGeometryBatchBuildData(materialProxy);
	geometryBatchData.batchElements.emplace_back(newBatchElement);
	geometryBatchData.meshletsNum += geometry.meshletsNum;
}

void GeometryBatchesBuilder::FinalizeBatches()
{
	SPT_PROFILER_FUNCTION();

	m_batches.geometryBatches.reserve(m_geometryBatchesData.size());

	for (const auto& [psoInfo, batchBuildData] : m_geometryBatchesData)
	{
		m_batches.geometryBatches.emplace_back(FinalizeBatchDefinition(psoInfo, batchBuildData));
	}
}

Uint16 GeometryBatchesBuilder::GetMaterialBatchIdx(const MaterialBatchPermutation& materialPermutation)
{
	SPT_CHECK(m_batches.materialBatches.size() <= maxValue<Uint16>);

	const auto [materialIt, wasEmplaced] = m_materialBatchesMap.try_emplace(materialPermutation, static_cast<Uint16>(m_batches.materialBatches.size()));

	if (wasEmplaced)
	{
		m_batches.materialBatches.emplace_back(materialPermutation);
	}

	return materialIt->second;
}

GeometryBatchesBuilder::GeometryBatchBuildData& GeometryBatchesBuilder::GetGeometryBatchBuildData(const mat::MaterialProxyComponent& materialProxy)
{
	return GetGeometryBatchBuildData(GetGeometryBatchPermutation(materialProxy));
}

GeometryBatchesBuilder::GeometryBatchBuildData& GeometryBatchesBuilder::GetGeometryBatchBuildData(const GeometryBatchPermutation& permutation)
{
	return m_geometryBatchesData[permutation];
}

GeometryBatchPermutation GeometryBatchesBuilder::GetGeometryBatchPermutation(const mat::MaterialProxyComponent& materialProxy) const
{
	GeometryBatchPermutation permutation;

	if(materialProxy.params.customOpacity)
	{
		permutation.SHADER         = materialProxy.params.shader;
		permutation.CUSTOM_OPACITY = true;

	}
	permutation.DOUBLE_SIDED = materialProxy.params.doubleSided;

	return permutation;
}

GeometryBatch GeometryBatchesBuilder::FinalizeBatchDefinition(const GeometryBatchPermutation& permutation, const GeometryBatchBuildData& batchBuildData) const
{
	SPT_CHECK(!batchBuildData.batchElements.empty());

	GeometryGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batchBuildData.batchElements.size());

	rhi::BufferDefinition batchElementsBufferDef;
	batchElementsBufferDef.size  = sizeof(rdr::HLSLStorage<GeometryBatchElement>) * batchBuildData.batchElements.size();
	batchElementsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	lib::SharedRef<rdr::Buffer> batchElementsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("GeometryBatchElements"), batchElementsBufferDef, rhi::EMemoryUsage::GPUOnly);

	rdr::UploadDataToBuffer(batchElementsBuffer, 0, reinterpret_cast<const Byte*>(batchBuildData.batchElements.data()), batchElementsBufferDef.size);

	lib::MTHandle<GeometryBatchDS> batchDS = rdr::ResourcesManager::CreateDescriptorSetState<GeometryBatchDS>(RENDERER_RESOURCE_NAME("GeometryBatchDS"));
	batchDS->u_batchElements = batchElementsBuffer->GetFullView();
	batchDS->u_batchData     = gpuBatchData;

	GeometryBatch newBatch;
	newBatch.batchElementsNum = static_cast<Uint32>(batchBuildData.batchElements.size());
	newBatch.batchMeshletsNum = batchBuildData.meshletsNum;
	newBatch.permutation      = permutation;
	newBatch.batchDS          = batchDS;

	return newBatch;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GeometryDrawer ================================================================================

GeometryDrawer::GeometryDrawer(GeometryPassDataCollection& batches)
	: m_batches(batches)
	, m_batchesBuilder(m_batches)
{ }

void GeometryDrawer::Draw(const GeometryDefinition& geometry, const mat::MaterialProxyComponent& materialProxy)
{
	m_batchesBuilder.AppendGeometry(geometry, materialProxy);
}

void GeometryDrawer::FinalizeDraws()
{
	m_batchesBuilder.FinalizeBatches();
}

} // spt::rsc
