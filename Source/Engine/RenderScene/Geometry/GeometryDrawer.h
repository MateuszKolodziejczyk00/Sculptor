#pragma once

#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"
#include "GeometryTypes.h"


namespace spt::rsc
{

struct GeometryDefinition
{
	Uint32 entityGPUIdx      = idxNone<Uint32>;
	Uint32 submeshGlobalIdx  = idxNone<Uint32>;
	Uint32 meshletsNum       = 0;
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

		lib::DynamicArray<GeometryBatchElement> batchElements;
		Uint32                                  meshletsNum = 0;
	};

	Uint16 GetMaterialBatchIdx(mat::MaterialShadersHash materialShadersHash);

	GeometryBatchBuildData& GetGeometryBatchBuildData(const mat::MaterialProxyComponent& materialProxy);
	GeometryBatchBuildData& GetGeometryBatchBuildData(GeometryBatchShader shader);
	GeometryBatchShader     GetGeometryBatchShader(const mat::MaterialProxyComponent& materialProxy) const;

	GeometryBatch FinalizeBatchDefinition(GeometryBatchShader shader, const GeometryBatchBuildData& batchBuildData) const;

	GeometryPassDataCollection& m_batches;

	lib::HashMap<mat::MaterialShadersHash, Uint16> m_materialBatchesMap;

	lib::StaticArray<GeometryBatchBuildData, GeometryBatchShader::GetDefaultShadersNum()> m_defaultGeometryBatchesData;
	lib::HashMap<mat::MaterialShadersHash, GeometryBatchBuildData>                        m_customGeometryBatchesData;
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
