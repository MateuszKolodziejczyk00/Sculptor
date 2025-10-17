#pragma once

#include "ShaderStructs/ShaderStructs.h"
#include "RenderSceneRegistry.h"
#include "GeometryTypes.h"


namespace spt::rsc
{

struct GeometryDefinition
{
	rsc::RenderEntityGPUPtr entityPtr;
	rsc::SubmeshGPUPtr      submeshPtr;
	Uint32                  meshletsNum = 0;
};


class GeometryBatchesBuilder
{
public:

	explicit GeometryBatchesBuilder(GeometryPassDataCollection& batchesCollector);

	void AppendGeometry(const GeometryDefinition& geometry, const mat::MaterialProxyComponent& materialProxy);

	void FinalizeBatches();

private:

	struct GeometryBatchBuildData
	{
		GeometryBatchBuildData()
		{ }

		Bool IsValid() const
		{
			return !batchElements.empty();
		}

		lib::DynamicArray<rdr::HLSLStorage<GeometryBatchElement>> batchElements;
		Uint32                                                    meshletsNum = 0u;
	};

	Uint16 GetMaterialBatchIdx(mat::MaterialShadersHash materialShadersHash);

	GeometryBatchBuildData& GetGeometryBatchBuildData(const mat::MaterialProxyComponent& materialProxy);
	GeometryBatchBuildData& GetGeometryBatchBuildData(const GeometryBatchPSOInfo& psoInfo);
	GeometryBatchPSOInfo    GetGeometryBatchPSOInfo(const mat::MaterialProxyComponent& materialProxy) const;

	GeometryBatch FinalizeBatchDefinition(const GeometryBatchPSOInfo& psoInfo, const GeometryBatchBuildData& batchBuildData) const;

	GeometryPassDataCollection& m_batches;

	lib::HashMap<mat::MaterialShadersHash, Uint16> m_materialBatchesMap;

	lib::HashMap<GeometryBatchPSOInfo, GeometryBatchBuildData, GeometryBatchPSOInfo::Hasher> m_geometryBatchesData;
};


class GeometryDrawer
{
public:

	explicit GeometryDrawer(GeometryPassDataCollection& batches);

	void Draw(const GeometryDefinition& geometry, const mat::MaterialProxyComponent& materialProxy);

	void FinalizeDraws();

	lib::Span<const GeometryBatch> GetGeometryBatches() const { return m_batches.geometryBatches; }
	lib::Span<const MaterialBatch> GetMaterialBatches() const { return m_batches.materialBatches; }

private:

	GeometryPassDataCollection& m_batches;
	GeometryBatchesBuilder      m_batchesBuilder;
};

} // spt::rsc
