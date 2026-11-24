#include "WorldShadowCacheRenderSystem.h"
#include "Lights/LightTypes.h"
#include "RenderSceneConstants.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "EngineFrame.h"
#include "SceneRenderer.h"
#include "ResourcesManager.h"


namespace spt::rsc::wsc
{

SPT_STATIC_CHECK([]
{
	for (const Uint32 tilesPerAxis : constants::cascadeTilesPerAxis)
	{
		if (tilesPerAxis == 0u || tilesPerAxis > constants::maxTilesPerAxis || constants::shadowMapRes % tilesPerAxis != 0u)
		{
			return false;
		}
	}
	return true;
}());

SPT_REGISTER_SCENE_RENDER_SYSTEM(WorldShadowCacheRenderSystem);

WorldShadowCacheRenderSystem::WorldShadowCacheRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene)
	: Super(arena, owningScene)
{
}

void WorldShadowCacheRenderSystem::Initialize(lib::MemoryArena& arena, RenderScene& renderScene)
{
	Super::Initialize(arena, renderScene);

	for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
	{
		InitializeCascade(m_cascades[cascadeIdx], cascadeIdx);
	}
}

void WorldShadowCacheRenderSystem::Update(const SceneUpdateContext& context)
{
	SPT_PROFILER_FUNCTION();

	Super::Update(context);

	const engn::FrameContext& frame = GetOwningScene().GetCurrentFrameRef();

	const Bool timeSlicingEnabled = GetOwningScene().settings.wsc.enableTimeSlicing;
	const Bool fullUpdate         = context.rendererSettings.resetAccumulation || timeSlicingEnabled != m_timeSlicingEnabled;
	m_timeSlicingEnabled = timeSlicingEnabled;

	if (fullUpdate)
	{
		Uint32 viewsNum = 0u;
		for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
		{
			const Uint32 tilesPerAxis = GetTilesPerAxis(cascadeIdx);
			viewsNum += tilesPerAxis * tilesPerAxis;
		}

		m_viewsToUpdate = frame.GetFrameMemoryArena().AllocateSpanUninitialized<RenderView*>(viewsNum);

		Uint32 viewIdx = 0u;
		for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
		{
			CascadeData& cascade = m_cascades[cascadeIdx];
			cascade.nextTileIdx = 0u;

			const Uint32 tilesPerAxis = GetTilesPerAxis(cascadeIdx);
			for (Uint32 tileIdx = 0u; tileIdx < tilesPerAxis * tilesPerAxis; ++tileIdx)
			{
				m_viewsToUpdate[viewIdx++] = RenderNextTile(context.mainRenderView, cascade, cascadeIdx, tilesPerAxis);
			}

			SPT_CHECK(cascade.nextTileIdx == 0u);
		}

		SPT_CHECK(viewIdx == viewsNum);
	}
	else
	{
		const Uint32 cascadeIdx = frame.GetFrameIdx() % constants::cascadeCount;

		m_viewsToUpdate = frame.GetFrameMemoryArena().AllocateSpanUninitialized<RenderView*>(1u);
		m_viewsToUpdate[0] = RenderNextTile(context.mainRenderView, m_cascades[cascadeIdx], cascadeIdx, GetTilesPerAxis(cascadeIdx));
	}
}

void WorldShadowCacheRenderSystem::UpdateGPUSceneData(const SceneUpdateContext& context, RenderSceneConstants& sceneData)
{
	Super::UpdateGPUSceneData(context, sceneData);

	WSCData wscData;

	for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
	{
		const CascadeData& cascade = m_cascades[cascadeIdx];
		wscData.dirLightsData.cascades[cascadeIdx].shadowMap = cascade.shadowMapsViews[cascade.frontIdx];
		wscData.dirLightsData.cascades[cascadeIdx].viewProj  = cascade.frontViewProj;
	}

	sceneData.wsc = wscData;
}

void WorldShadowCacheRenderSystem::CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector)
{
	Super::CollectRenderViews(rendererInterface, renderScene, mainRenderView, INOUT viewsCollector);

	for (RenderView* tileView : m_viewsToUpdate)
	{
		viewsCollector.AddRenderView(*tileView);
	}
}

void WorldShadowCacheRenderSystem::InitializeCascade(CascadeData& cascade, Uint32 cascadeIdx) const
{
	const math::Vector2u shadowMapRes(constants::shadowMapRes, constants::shadowMapRes);

	for (Uint32 bufferIdx = 0u; bufferIdx < 2u; ++bufferIdx)
	{
		rhi::TextureDefinition shadowMapDef;
		shadowMapDef.resolution = shadowMapRes;
		shadowMapDef.format     = rhi::EFragmentFormat::D32_S_Float;
		shadowMapDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::DepthSetncilRT);
		shadowMapDef.flags      = rhi::ETextureFlags::GloballyReadable;

		const lib::SharedRef<rdr::Texture> shadowMap = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("World Shadow Cache Cascade Shadow Map"), shadowMapDef, rhi::EMemoryUsage::GPUOnly);

		cascade.shadowMaps[bufferIdx]      = shadowMap;
		cascade.shadowMapsViews[bufferIdx] = shadowMap->CreateView(RENDERER_RESOURCE_NAME("World Shadow Cache Cascade Shadow Map View"));
	}

	const Uint32 tilesPerAxis = constants::cascadeTilesPerAxis[cascadeIdx];

	for (Uint32 tileIdx = 0u; tileIdx < tilesPerAxis * tilesPerAxis; ++tileIdx)
	{
		RenderViewDefinition tileViewDef;
		tileViewDef.renderStages = ERenderStage::ShadowMapRendererStages;
		RenderView* tileView = CreateRenderView(tileViewDef);

		ShadowMapViewComponent shadowMapViewComponent;
		shadowMapViewComponent.shadowMapType     = EShadowMapType::DirectionalLightCascade;
		shadowMapViewComponent.faceIdx           = 0u;
		shadowMapViewComponent.techniqueOverride = EShadowMappingTechnique::CSM;

		tileView->GetBlackboard().Create<ShadowMapViewComponent>(shadowMapViewComponent);

		cascade.tileViews[tileIdx] = tileView;
	}
}

void WorldShadowCacheRenderSystem::BeginCycle(const RenderView& mainView, CascadeData& cascade, Uint32 cascadeIdx, Uint32 tilesPerAxis) const
{
	SPT_CHECK(tilesPerAxis > 0u && tilesPerAxis <= constants::cascadeTilesPerAxis[cascadeIdx]);
	SPT_CHECK(cascade.nextTileIdx == 0u);

	const math::Vector3f lightDirection = GetOwningScene().lighting.GetDirectionalLight().direction;

	constexpr lib::StaticArray<Real32, constants::cascadeCount> cascadeExtents = { 100.f, 400.f };
	const Real32 extent = cascadeExtents[cascadeIdx];

	const Real32 depthRange = 1000.f;

	SceneView cascadeView;
	cascadeView.SetRotation(lightDirection);
	const math::Quaternionf rotation = cascadeView.GetRotation();

	// Snapping to texels keeps the content of the swapped shadow map stable when the cascade moves
	const Real32 texelSize = 2.f * extent / constants::shadowMapRes;
	math::Vector3f lightSpaceLocation = rotation.conjugate() * mainView.GetLocation();
	lightSpaceLocation.y() = std::round(lightSpaceLocation.y() / texelSize) * texelSize;
	lightSpaceLocation.z() = std::round(lightSpaceLocation.z() / texelSize) * texelSize;

	const math::Vector3f location = rotation * lightSpaceLocation - lightDirection * 0.7f * depthRange;

	cascadeView.SetLocation(location);
	cascadeView.SetOrthographicsProjection(0.f, depthRange, -extent, extent, -extent, extent);
	cascade.pendingViewProj = cascadeView.GenerateViewProjectionMatrix();

	const Real32 tileExtent = extent / tilesPerAxis;
	const Uint32 tileRes    = constants::shadowMapRes / tilesPerAxis;

	for (Uint32 tileY = 0u; tileY < tilesPerAxis; ++tileY)
	{
		for (Uint32 tileX = 0u; tileX < tilesPerAxis; ++tileX)
		{
			RenderView& tileView = *cascade.tileViews[tileY * tilesPerAxis + tileX];

			// Shadow map texel rows go along view space -Z
			const math::Vector3f tileCenter(0.f, -extent + (2u * tileX + 1u) * tileExtent, extent - (2u * tileY + 1u) * tileExtent);

			tileView.SetLocation(location + rotation * tileCenter);
			tileView.SetRotation(rotation);
			tileView.SetOrthographicsProjection(0.f, depthRange, -tileExtent, tileExtent, -tileExtent, tileExtent);
			tileView.SetOutputRes(math::Vector2u(tileRes, tileRes));

			tileView.GetBlackboard().Get<ShadowMapViewComponent>().tileOffset = math::Vector2u(tileX * tileRes, tileY * tileRes);
		}
	}
}

RenderView* WorldShadowCacheRenderSystem::RenderNextTile(const RenderView& mainView, CascadeData& cascade, Uint32 cascadeIdx, Uint32 tilesPerAxis) const
{
	if (cascade.nextTileIdx == 0u)
	{
		BeginCycle(mainView, cascade, cascadeIdx, tilesPerAxis);
	}

	RenderView* tileView = cascade.tileViews[cascade.nextTileIdx++];
	tileView->GetBlackboard().Get<ShadowMapViewComponent>().shadowMap = cascade.shadowMaps[1u - cascade.frontIdx];

	if (cascade.nextTileIdx == tilesPerAxis * tilesPerAxis)
	{
		cascade.frontIdx      = 1u - cascade.frontIdx;
		cascade.frontViewProj = cascade.pendingViewProj;
		cascade.nextTileIdx   = 0u;
	}

	return tileView;
}

Uint32 WorldShadowCacheRenderSystem::GetTilesPerAxis(Uint32 cascadeIdx) const
{
	return m_timeSlicingEnabled ? constants::cascadeTilesPerAxis[cascadeIdx] : 1u;
}

WorldShadowCacheRenderSystem::CascadeData::~CascadeData()
{
	for (RenderView*& tileView : tileViews)
	{
		if (tileView)
		{
			DestroyRenderView(tileView);
		}
	}
}

} // spt::rsc::wsc
