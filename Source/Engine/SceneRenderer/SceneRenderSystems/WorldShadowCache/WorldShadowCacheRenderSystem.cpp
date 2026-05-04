#include "WorldShadowCacheRenderSystem.h"
#include "Lights/LightTypes.h"
#include "RenderSceneConstants.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "EngineFrame.h"
#include "SceneRenderer.h"


namespace spt::rsc::wsc
{

SPT_REGISTER_SCENE_RENDER_SYSTEM(WorldShadowCacheRenderSystem);

WorldShadowCacheRenderSystem::WorldShadowCacheRenderSystem(RenderScene& owningScene)
	: Super(owningScene)
{
}

void WorldShadowCacheRenderSystem::Initialize(lib::MemoryArena& arena, RenderScene& renderScene)
{
	Super::Initialize(arena, renderScene);

	m_dirLightData = CreatePerLightData();
}

void WorldShadowCacheRenderSystem::Update(const SceneUpdateContext& context)
{
	SPT_PROFILER_FUNCTION();

	Super::Update(context);

	const engn::FrameContext& frame = GetOwningScene().GetCurrentFrameRef();

	const Bool fullUpdate = context.rendererSettings.resetAccumulation;

	const SizeType cascadesToUpdateNum = fullUpdate ? constants::cascadeCount : 1u;
	m_cascadesToUpdate = frame.GetFrameMemoryArena().AllocateSpanUninitialized<RenderView*>(cascadesToUpdateNum);

	Uint32 cascadeUpdateIdx = 0u;
	if (fullUpdate)
	{
		for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
		{
			UpdateCascadeViewsMatrices(context.mainRenderView, m_dirLightData, cascadeIdx);
			m_cascadesToUpdate[cascadeUpdateIdx++] = m_dirLightData.shadowCascadeViews[cascadeIdx];
		}
	}
	else
	{
		const Uint32 cascadeIdx = frame.GetFrameIdx() % constants::cascadeCount;
		UpdateCascadeViewsMatrices(context.mainRenderView, m_dirLightData, cascadeIdx);
		m_cascadesToUpdate[cascadeUpdateIdx++] = m_dirLightData.shadowCascadeViews[cascadeIdx];
	}
}

void WorldShadowCacheRenderSystem::UpdateGPUSceneData(RenderSceneConstants& sceneData)
{
	Super::UpdateGPUSceneData(sceneData);

	WSCData wscData;

	for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
	{
		wscData.dirLightsData.cascades[cascadeIdx].shadowMap = m_dirLightData.shadowMapsViews[cascadeIdx];
		wscData.dirLightsData.cascades[cascadeIdx].viewProj  = m_dirLightData.shadowCascadeViews[cascadeIdx]->GenerateViewProjectionMatrix();
	}

	sceneData.wsc = wscData;
}

void WorldShadowCacheRenderSystem::CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector)
{
	Super::CollectRenderViews(rendererInterface, renderScene, mainRenderView, INOUT viewsCollector);

	for (RenderView* cascadeView : m_cascadesToUpdate)
	{
		viewsCollector.AddRenderView(*cascadeView);
	}
}

WorldShadowCacheRenderSystem::PerLightData WorldShadowCacheRenderSystem::CreatePerLightData() const
{
	PerLightData lightData;

	const math::Vector2u shadowMapRes(2048u, 2048u);

	for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
	{
		rhi::TextureDefinition shadowMapDef;
		shadowMapDef.resolution = shadowMapRes;
		shadowMapDef.format     = rhi::EFragmentFormat::D32_S_Float;
		shadowMapDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::DepthSetncilRT);
		shadowMapDef.flags      = rhi::ETextureFlags::GloballyReadable;

		const lib::SharedRef<rdr::Texture> shadowMap = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("World Shadow Cache Cascade Shadow Map"), shadowMapDef, rhi::EMemoryUsage::GPUOnly);

		lightData.shadowMaps[cascadeIdx]      = shadowMap;
		lightData.shadowMapsViews[cascadeIdx] = shadowMap->CreateView(RENDERER_RESOURCE_NAME("World Shadow Cache Cascade Shadow Map View"));

		RenderViewDefinition cascadeViewDef;
		cascadeViewDef.renderStages = ERenderStage::ShadowMapRendererStages;
		RenderView* cascadeView = CreateRenderView(cascadeViewDef);
		cascadeView->SetOutputRes(shadowMapRes);

		ShadowMapViewComponent shadowMapViewComponent;
		shadowMapViewComponent.shadowMap         = shadowMap;
		shadowMapViewComponent.shadowMapType     = EShadowMapType::DirectionalLightCascade;
		shadowMapViewComponent.faceIdx           = 0u;
		shadowMapViewComponent.techniqueOverride = EShadowMappingTechnique::CSM;

		cascadeView->GetBlackboard().Create<ShadowMapViewComponent>(shadowMapViewComponent);

		lightData.shadowCascadeViews[cascadeIdx] = std::move(cascadeView);
	}

	return lightData;
}

void WorldShadowCacheRenderSystem::UpdateCascadeViewsMatrices(const RenderView& mainView, PerLightData& dirLightShadowsData, Uint32 cascadeIdx) const
{
	const DirectionalLightData& dirLightData = GetOwningScene().lighting.GetDirectionalLight();
	const math::Vector3f lightDirection = dirLightData.direction;

	RenderView& cascadeView = *dirLightShadowsData.shadowCascadeViews[cascadeIdx];

	const math::Vector3f mainViewLocation = mainView.GetLocation();

	constexpr lib::StaticArray<Real32, constants::cascadeCount> cascadeExtents = { 100.f, 400.f };
	const Real32 extent = cascadeExtents[cascadeIdx];

	const Real32 depthRange = 1000.f;

	cascadeView.SetLocation(mainViewLocation - lightDirection * 0.7f * depthRange);
	cascadeView.SetRotation(lightDirection);

	cascadeView.SetOrthographicsProjection(0.f, depthRange, -extent, extent, -extent, extent);
}

WorldShadowCacheRenderSystem::PerLightData::~PerLightData()
{
	for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
	{
		if (shadowCascadeViews[cascadeIdx])
		{
			DestroyRenderView(shadowCascadeViews[cascadeIdx]);
		}
	}
}

} // spt::rsc::wsc
