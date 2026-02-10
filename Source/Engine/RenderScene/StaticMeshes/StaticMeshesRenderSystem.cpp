#include "StaticMeshesRenderSystem.h"
#include "Geometry/GeometryDrawer.h"
#include "Lights/LightTypes.h"
#include "RenderScene.h"
#include "StaticMeshes/RenderMesh.h"
#include "StaticMeshesRenderSystem.h"
#include "Transfers/UploadUtils.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// SMBatchesBuilder ==============================================================================

class SMBatchesBuilder
{
public:

	SMBatchesBuilder(lib::DynamicArray<StaticMeshSMBatchDefinition>& inBatches);

	void AppendMeshToBatch(RenderEntityGPUPtr entityPtr, const RenderMesh& mesh, const rsc::MaterialSlotsComponent& materialsSlots);

	void FinalizeBatches();

private:


	struct BatchBuildData
	{
		BatchBuildData()
			: maxMeshletsNum(0)
			, maxTrianglesNum(0)
		{ }

		Bool IsValid() const
		{
			return !batchElements.empty();
		}

		lib::DynamicArray<StaticMeshBatchElement> batchElements;
		Uint32 maxMeshletsNum;
		Uint32 maxTrianglesNum;
		SMDepthOnlyPermutation permutation;
	};


	BatchBuildData& GetBatchBuildDataForMaterial(const SMDepthOnlyPermutation& permutation);

	StaticMeshSMBatchDefinition FinalizeBatchDefinition(const BatchBuildData& batchBuildData) const;

	using PermutationsMap = lib::HashMap<SMDepthOnlyPermutation, Uint32, rdr::ShaderStructHasher<SMDepthOnlyPermutation>>;

	PermutationsMap m_permutationToBatchIdx;

	lib::DynamicArray<BatchBuildData>             m_batchBuildDatas;
	lib::DynamicArray<StaticMeshSMBatchDefinition>& m_batches;
};


SMBatchesBuilder::SMBatchesBuilder(lib::DynamicArray<StaticMeshSMBatchDefinition>& inBatches)
	: m_batches(inBatches)
{ }

void SMBatchesBuilder::AppendMeshToBatch(RenderEntityGPUPtr entityPtr, const RenderMesh& mesh, const rsc::MaterialSlotsComponent& materialsSlots)
{
	const lib::Span<const SubmeshRenderingDefinition> submeshes = mesh.GetSubmeshes();

	for (Uint32 idx = 0; idx < submeshes.size(); ++idx)
	{
		const ecs::EntityHandle material = materialsSlots.slots[idx];
		const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

		if (materialProxy.params.transparent)
		{
			continue;
		}

		const SubmeshRenderingDefinition& submeshDef = submeshes[idx];

		SMDepthOnlyPermutation permutation;

		if (materialProxy.params.customOpacity) // No material evaluation needed if custom opacity is not used
		{
			permutation.SHADER         = materialProxy.params.shader;
			permutation.CUSTOM_OPACITY = materialProxy.params.customOpacity;
		}

		BatchBuildData& batch = GetBatchBuildDataForMaterial(permutation);

		const SubmeshGPUPtr submeshPtr = mesh.GetSubmeshesGPUPtr() + idx;

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
// StaticMeshesRenderSystem ======================================================================

StaticMeshesRenderSystem::StaticMeshesRenderSystem(RenderScene& owningScene)
	: Super(owningScene)
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DepthPrepass, ERenderStage::ShadowMap);
}

void StaticMeshesRenderSystem::Update()
{
	SPT_PROFILER_FUNCTION();

	Super::Update();

	m_cachedSMBatches = CacheStaticMeshBatches();
}

void StaticMeshesRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs, settings);

	{
		lib::DynamicArray<ViewRenderingSpec*> shadowMapViewSpecs;
		std::copy_if(std::cbegin(viewSpecs), std::cend(viewSpecs),
					 std::back_inserter(shadowMapViewSpecs),
					 [](const ViewRenderingSpec* viewSpec)
					 {
						 return viewSpec->SupportsStage(ERenderStage::ShadowMap);
					 });

		m_shadowMapRenderer.RenderPerFrame(graphBuilder, renderScene, shadowMapViewSpecs);
	}

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		SPT_CHECK(!!viewSpec);
		RenderPerView(graphBuilder, renderScene, *viewSpec);
	}
}

void StaticMeshesRenderSystem::FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	Super::FinishRenderingFrame(graphBuilder, renderScene);

	m_shadowMapRenderer.FinishRenderingFrame();
}

const lib::DynamicArray<StaticMeshSMBatchDefinition>& StaticMeshesRenderSystem::BuildBatchesForSMView(const RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

	return m_cachedSMBatches.batches;
}

lib::DynamicArray<StaticMeshSMBatchDefinition> StaticMeshesRenderSystem::BuildBatchesForPointLightSM(const PointLightData& pointLight) const
{
	SPT_PROFILER_FUNCTION();
	
	lib::DynamicArray<StaticMeshSMBatchDefinition> batches;

	SMBatchesBuilder batchesBuilder(batches);

	const auto meshesView = GetOwningScene().GetRegistry().view<TransformComponent, StaticMeshInstanceRenderData, EntityGPUDataHandle, rsc::MaterialSlotsComponent>();
	for (const auto& [entity, transformComp, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const RenderMesh* mesh = staticMeshRenderData.staticMesh.Get();
		SPT_CHECK(!!mesh);

		const math::Vector3f boundingSphereCenterWS = transformComp.GetTransform() * mesh->GetBoundingSphereCenter();
		const Real32 boundingSphereRadius = mesh->GetBoundingSphereRadius() * transformComp.GetUniformScale();

		if ((boundingSphereCenterWS - pointLight.location).squaredNorm() < math::Utils::Square(pointLight.radius + boundingSphereRadius))
		{
			batchesBuilder.AppendMeshToBatch(entityGPUDataHandle.GetGPUDataPtr(), *mesh, materialsSlots);
		}
	}

	batchesBuilder.FinalizeBatches();

	return batches;
}

const GeometryPassDataCollection& StaticMeshesRenderSystem::GetCachedOpaqueGeometryPassData() const
{
	return m_cachedSMBatches.opaqueGeometryPassData;
}

const GeometryPassDataCollection& StaticMeshesRenderSystem::GetCachedTransparentGeometryPassData() const
{
	return m_cachedSMBatches.transparentGeometryPassData;
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();
	
	if (viewSpec.SupportsStage(ERenderStage::ShadowMap))
	{
		RenderStageEntries& shadowMapStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ShadowMap);
		shadowMapStageEntries.GetOnRenderStage().AddRawMember(&m_shadowMapRenderer, &StaticMeshShadowMapRenderer::RenderToShadowMap);
	}
}

StaticMeshesRenderSystem::CachedSMBatches StaticMeshesRenderSystem::CacheStaticMeshBatches() const
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
		const RenderMesh* mesh = staticMeshRenderData.staticMesh.Get();
		SPT_CHECK(!!mesh);

		const RenderEntityGPUPtr entityPtr = entityGPUDataHandle.GetGPUDataPtr();
		batchesBuilder.AppendMeshToBatch(entityPtr, *mesh, materialsSlots);
	}

	// New system
	for (const auto& [entity, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const RenderMesh* mesh = staticMeshRenderData.staticMesh.Get();
		SPT_CHECK(!!mesh);

		const lib::Span<const SubmeshRenderingDefinition> submeshes = mesh->GetSubmeshes();

		for (Uint32 idx = 0; idx < submeshes.size(); ++idx)
		{
			const ecs::EntityHandle material = materialsSlots.slots[idx];
			const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

			const SubmeshRenderingDefinition& submeshDef = submeshes[idx];

			GeometryDefinition geometryDef;
			geometryDef.entityPtr   = entityGPUDataHandle.GetGPUDataPtr();
			geometryDef.submeshPtr  = mesh->GetSubmeshesGPUPtr() + idx;
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
