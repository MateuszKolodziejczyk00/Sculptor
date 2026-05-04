#include "StaticMeshesRenderSystem.h"
#include "Utils/Geometry/GeometryDrawer.h"
#include "Lights/LightTypes.h"
#include "RenderScene.h"
#include "StaticMeshes/RenderMesh.h"
#include "StaticMeshesRenderSystem.h"
#include "Utils/TransfersUtils.h"


namespace spt::rsc
{

SPT_REGISTER_SCENE_RENDER_SYSTEM(StaticMeshesRenderSystem);

//////////////////////////////////////////////////////////////////////////////////////////////////
// StaticMeshesRenderSystem ======================================================================

StaticMeshesRenderSystem::StaticMeshesRenderSystem(RenderScene& owningScene)
	: Super(owningScene)
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DepthPrepass, ERenderStage::ShadowMap);
}

void StaticMeshesRenderSystem::Update(const SceneUpdateContext& context)
{
	SPT_PROFILER_FUNCTION();

	Super::Update(context);

	m_cachedSMBatches = CacheStaticMeshBatches();
}

void StaticMeshesRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	Super::RenderPerFrame(graphBuilder, rendererInterface, renderScene, viewSpecs, settings);

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		SPT_CHECK(!!viewSpec);
		RenderPerView(graphBuilder, renderScene, *viewSpec);
	}
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
		// TODO: As part of refactor, point light shadow maps is not supported for some time
	}
}

StaticMeshesRenderSystem::CachedSMBatches StaticMeshesRenderSystem::CacheStaticMeshBatches() const
{
	SPT_PROFILER_FUNCTION();

	CachedSMBatches cachedBatches;

	GeometryDrawer opaqueGeometryDrawer(OUT cachedBatches.opaqueGeometryPassData);
	GeometryDrawer transparentGeometryDrawer(OUT cachedBatches.transparentGeometryPassData);

	// New system
	RenderScene& scene = GetOwningScene();

	const auto addDraw = [&](const RetainedDrawHandle handle, const RetainedDraw& draw)
	{
		const lib::Span<const SubmeshRenderingDefinition> submeshes = draw.mesh.GetSubmeshes();

		MaterialSlotsChunk* currentSlotsChunk = scene.materials.slots.Get(draw.materialSlots);
		Uint32 matSlotInChunkIdx = 0u;

		for (Uint32 idx = 0; idx < submeshes.size(); ++idx)
		{
			const ecs::EntityHandle material = currentSlotsChunk->slots[matSlotInChunkIdx++];
			const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

			const SubmeshRenderingDefinition& submeshDef = submeshes[idx];

			GeometryDefinition geometryDef;
			geometryDef.entityPtr   = GetInstanceGPUDataPtr(draw.instance);
			geometryDef.submeshPtr  = draw.mesh.GetSubmeshesGPUPtr() + idx;
			geometryDef.meshletsNum = submeshDef.meshletsNum;

			if (materialProxy.params.transparent)
			{
				transparentGeometryDrawer.Draw(geometryDef, materialProxy);
			}
			else
			{
				opaqueGeometryDrawer.Draw(geometryDef, materialProxy);
			}

			if (matSlotInChunkIdx >= currentSlotsChunk->slots.size())
			{
				currentSlotsChunk = scene.materials.slots.Get(currentSlotsChunk->next);
				matSlotInChunkIdx = 0u;
			}
		}
	};

	GetOwningScene().draws.draws.ForEach(addDraw);

	opaqueGeometryDrawer.FinalizeDraws();
	transparentGeometryDrawer.FinalizeDraws();

	return cachedBatches;
}

} // spt::rsc
