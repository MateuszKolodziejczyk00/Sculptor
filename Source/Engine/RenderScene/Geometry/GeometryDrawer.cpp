#include "GeometryDrawer.h"
#include "Transfers/UploadUtils.h"


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
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(geometry.entityGPUIdx != idxNone<Uint32>);
	SPT_CHECK(geometry.submeshGlobalIdx != idxNone<Uint32>);
	SPT_CHECK(geometry.meshletsNum > 0u);

	const Uint16 materialBatchIdx = GetMaterialBatchIdx(materialProxy.materialShadersHash);

	GeometryBatchElement newBatchElement;
	newBatchElement.entityIdx        = geometry.entityGPUIdx;
	newBatchElement.submeshGlobalIdx = geometry.submeshGlobalIdx;
	newBatchElement.materialDataID   = materialProxy.GetMaterialDataID();
	newBatchElement.materialBatchIdx = materialBatchIdx;

	GeometryBatchBuildData& geometryBatchData = GetGeometryBatchBuildData(materialProxy);
	geometryBatchData.batchElements.emplace_back(newBatchElement);
	geometryBatchData.meshletsNum += geometry.meshletsNum;
}

void GeometryBatchesBuilder::FinalizeBatches()
{
	SPT_PROFILER_FUNCTION();

	m_batches.geometryBatches.reserve(m_defaultGeometryBatchesData.size() + m_customGeometryBatchesData.size());

	for (SizeType i = 0; i < m_defaultGeometryBatchesData.size(); ++i)
	{
		const GeometryBatchShader shader = GeometryBatchShader::EDefault(i);
		const GeometryBatchBuildData& batchBuildData = m_defaultGeometryBatchesData[i];
		if (batchBuildData.IsValid())
		{
			m_batches.geometryBatches.emplace_back(FinalizeBatchDefinition(shader, batchBuildData));
		}
	}

	for (const auto& [shader, batchBuildData] : m_customGeometryBatchesData)
	{
		m_batches.geometryBatches.emplace_back(FinalizeBatchDefinition(GeometryBatchShader(shader), batchBuildData));
	}
}

Uint16 GeometryBatchesBuilder::GetMaterialBatchIdx(mat::MaterialShadersHash materialShadersHash)
{
	SPT_CHECK(m_batches.materialBatches.size() <= maxValue<Uint16>);

	const auto [materialIt, wasEmplaced] = m_materialBatchesMap.try_emplace(materialShadersHash, static_cast<Uint16>(m_batches.materialBatches.size()));

	if (wasEmplaced)
	{
		m_batches.materialBatches.emplace_back(materialShadersHash);
	}

	return materialIt->second;
}

GeometryBatchesBuilder::GeometryBatchBuildData& GeometryBatchesBuilder::GetGeometryBatchBuildData(const mat::MaterialProxyComponent& materialProxy)
{
	return GetGeometryBatchBuildData(GetGeometryBatchShader(materialProxy));
}

GeometryBatchesBuilder::GeometryBatchBuildData& GeometryBatchesBuilder::GetGeometryBatchBuildData(GeometryBatchShader shader)
{
	if (shader.IsDefaultShader())
	{
		return m_defaultGeometryBatchesData[static_cast<SizeType>(shader.GetDefaultShader())];
	}
	else
	{
		return m_customGeometryBatchesData[shader.GetCustomShader()];
	}
}

GeometryBatchShader GeometryBatchesBuilder::GetGeometryBatchShader(const mat::MaterialProxyComponent& materialProxy) const
{
	if (!materialProxy.params.customOpacity)
	{
		return GeometryBatchShader::EDefault::Opaque;
	}
	else
	{
		return GeometryBatchShader(materialProxy.materialShadersHash);
	}
}

GeometryBatch GeometryBatchesBuilder::FinalizeBatchDefinition(GeometryBatchShader shader, const GeometryBatchBuildData& batchBuildData) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!batchBuildData.batchElements.empty());

	GeometryGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batchBuildData.batchElements.size());

	rhi::BufferDefinition batchElementsBufferDef;
	batchElementsBufferDef.size  = sizeof(GeometryBatchElement) * batchBuildData.batchElements.size();
	batchElementsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	lib::SharedRef<rdr::Buffer> batchElementsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("GeometryBatchElements"), batchElementsBufferDef, rhi::EMemoryUsage::GPUOnly);

	gfx::UploadDataToBuffer(batchElementsBuffer, 0, reinterpret_cast<const Byte*>(batchBuildData.batchElements.data()), batchElementsBufferDef.size);

	lib::MTHandle<GeometryBatchDS> batchDS = rdr::ResourcesManager::CreateDescriptorSetState<GeometryBatchDS>(RENDERER_RESOURCE_NAME("GeometryBatchDS"));
	batchDS->u_batchElements = batchElementsBuffer->CreateFullView();
	batchDS->u_batchData     = gpuBatchData;

	GeometryBatch newBatch;
	newBatch.batchElementsNum = static_cast<Uint32>(batchBuildData.batchElements.size());
	newBatch.batchMeshletsNum = batchBuildData.meshletsNum;
	newBatch.shader           = shader;
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
