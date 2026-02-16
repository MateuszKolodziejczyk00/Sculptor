#include "WorldShadowCacheRenderSystem.h"
#include "Lights/LightTypes.h"
#include "RenderSceneConstants.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "EngineFrame.h"

#pragma optimize("", off)
namespace spt::rsc::wsc
{


WorldShadowCacheRenderSystem::WorldShadowCacheRenderSystem(RenderScene& owningScene)
	: Super(owningScene)
{
}

void WorldShadowCacheRenderSystem::Update(const SceneUpdateContext& context)
{
	SPT_PROFILER_FUNCTION();

	Super::Update(context);

	const SizeType dirLightsNum = m_perLightData.size();
	if (dirLightsNum == 0)
	{
		return;
	}

	const engn::FrameContext& frame = GetOwningScene().GetCurrentFrameRef();

	const Bool fullUpdate = context.rendererSettings.resetAccumulation;

	const SizeType cascadesToUpdateNum = dirLightsNum * (fullUpdate ? constants::cascadeCount : 1u);
	m_cascadesToUpdate = frame.GetFrameMemoryArena().AllocateSpanUninitialized<RenderView*>(cascadesToUpdateNum);

	Uint32 cascadeUpdateIdx = 0u;
	for (auto& dirLightData : m_perLightData)
	{
		if (fullUpdate)
		{
			for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
			{
				UpdateCascadeViewsMatrices(context.mainRenderView, dirLightData, cascadeIdx);
				m_cascadesToUpdate[cascadeUpdateIdx++] = dirLightData.shadowCascadeViews[cascadeIdx].get();
			}
		}
		else
		{
			const Uint32 cascadeIdx = frame.GetFrameIdx() % constants::cascadeCount;
			UpdateCascadeViewsMatrices(context.mainRenderView, dirLightData, cascadeIdx);
			m_cascadesToUpdate[cascadeUpdateIdx++] = dirLightData.shadowCascadeViews[cascadeIdx].get();
		}
	}
}

void WorldShadowCacheRenderSystem::UpdateGPUSceneData(RenderSceneConstants& sceneData)
{
	Super::UpdateGPUSceneData(sceneData);

	WSCData wscData;
	SPT_CHECK(m_perLightData.size() <= 1u); // currently support only one directional light, so only one shadow cache

	if (!m_perLightData.empty())
	{
		const PerLightData& dirLightData = m_perLightData.front();

		for (Uint32 cascadeIdx = 0u; cascadeIdx < constants::cascadeCount; ++cascadeIdx)
		{
			wscData.dirLightsData.cascades[cascadeIdx].shadowMap = dirLightData.shadowMapsViews[cascadeIdx];
			wscData.dirLightsData.cascades[cascadeIdx].viewProj  = dirLightData.shadowCascadeViews[cascadeIdx]->GenerateViewProjectionMatrix();
		}
	}

	sceneData.wsc = wscData;
}

void WorldShadowCacheRenderSystem::CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector)
{
	Super::CollectRenderViews(renderScene, mainRenderView, INOUT viewsCollector);

	for (RenderView* cascadeView : m_cascadesToUpdate)
	{
		viewsCollector.AddRenderView(*cascadeView);
	}
}

void WorldShadowCacheRenderSystem::OnInitialize(RenderScene& renderScene)
{
	Super::OnInitialize(renderScene);

	RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

	sceneRegistry.on_construct<DirectionalLightData>().connect<&WorldShadowCacheRenderSystem::OnDirectionalLightAdded>(this);
	sceneRegistry.on_destroy<DirectionalLightData>().connect<&WorldShadowCacheRenderSystem::OnDirectionalLightRemoved>(this);
}

void WorldShadowCacheRenderSystem::OnDeinitialize(RenderScene& renderScene)
{
	Super::OnDeinitialize(renderScene);

	RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

	sceneRegistry.on_construct<DirectionalLightData>().disconnect<&WorldShadowCacheRenderSystem::OnDirectionalLightAdded>(this);
	sceneRegistry.on_destroy<DirectionalLightData>().disconnect<&WorldShadowCacheRenderSystem::OnDirectionalLightRemoved>(this);
}

void WorldShadowCacheRenderSystem::OnDirectionalLightAdded(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	PerLightData lightData;
	lightData.dirLightEntity = entity;

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

		lib::SharedRef<RenderView> cascadeView = lib::MakeShared<RenderView>(GetOwningScene());
		cascadeView->AddRenderStages(ERenderStage::ShadowMapRendererStages);
		cascadeView->SetRenderingRes(shadowMapRes);

		ShadowMapViewComponent shadowMapViewComponent;
		shadowMapViewComponent.shadowMap         = shadowMap;
		shadowMapViewComponent.owningLight       = entity;
		shadowMapViewComponent.shadowMapType     = EShadowMapType::DirectionalLightCascade;
		shadowMapViewComponent.faceIdx           = 0u;
		shadowMapViewComponent.techniqueOverride = EShadowMappingTechnique::CSM;

		cascadeView->GetBlackboard().Create<ShadowMapViewComponent>(shadowMapViewComponent);

		lightData.shadowCascadeViews[cascadeIdx] = std::move(cascadeView);
	}

	m_perLightData.emplace_back(std::move(lightData));
}


void WorldShadowCacheRenderSystem::OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	lib::RemoveAllBy(m_perLightData, [entity](const PerLightData& data) { return data.dirLightEntity == entity; });
}

void WorldShadowCacheRenderSystem::UpdateCascadeViewsMatrices(const RenderView& mainView, PerLightData& dirLightShadowsData, Uint32 cascadeIdx) const
{
	const DirectionalLightData& dirLightData = GetOwningScene().GetRegistry().get<DirectionalLightData>(dirLightShadowsData.dirLightEntity);
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

} // spt::rsc::wsc
