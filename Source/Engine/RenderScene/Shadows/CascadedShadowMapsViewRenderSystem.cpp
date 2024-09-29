#include "CascadedShadowMapsViewRenderSystem.h"
#include "View/RenderView.h"
#include "ResourcesManager.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"


namespace spt::rsc
{

CascadedShadowMapsViewRenderSystem::CascadedShadowMapsViewRenderSystem(const ShadowCascadesParams& params)
	: m_params(params)
{ }

void CascadedShadowMapsViewRenderSystem::CollectRenderViews(const RenderScene& renderScene, const RenderView& owningView, INOUT RenderViewsCollector& viewsCollector)
{
	Super::CollectRenderViews(renderScene, owningView, INOUT viewsCollector);

	if (!m_dirLightShadowCascades.empty())
	{
		const CascadeViewsUpdateParams cascadeViewsUpdateParams = ComputeCascadeViewsUpdateParams(owningView);

		for (DirLightShadowCascades& cascades : m_dirLightShadowCascades)
		{
			UpdateCascadeViewsMatrices(owningView, cascades, cascadeViewsUpdateParams);
			CollectCascadeViews(cascades, INOUT viewsCollector);
		}
	}
}

const lib::DynamicArray<ShadowMap>& CascadedShadowMapsViewRenderSystem::GetShadowCascadesViews(RenderSceneEntity directionalLightEntity) const
{
	const auto foundCascades = std::find_if(std::cbegin(m_dirLightShadowCascades), std::cend(m_dirLightShadowCascades),
											[directionalLightEntity](const DirLightShadowCascades& cascades)
											{
												return cascades.dirLightEntity.entity() == directionalLightEntity;
											});

	SPT_CHECK_MSG(foundCascades != std::cend(m_dirLightShadowCascades), "No shadow cascades found for directional light!");
	return foundCascades->cascadesShadowMaps;
}

void CascadedShadowMapsViewRenderSystem::OnInitialize(RenderView& inRenderView)
{
	Super::OnInitialize(inRenderView);
	
	RenderScene& owningScene = GetOwningView().GetRenderScene();
	RenderSceneRegistry& sceneRegistry = owningScene.GetRegistry();

	for(const auto& [entity, directionalLightComponent] : sceneRegistry.view<DirectionalLightData>().each())
	{
		m_dirLightShadowCascades.emplace_back(CreateDirLightShadowCascades(owningScene, RenderSceneEntityHandle(sceneRegistry, entity)));
	}

	sceneRegistry.on_construct<DirectionalLightData>().connect<&CascadedShadowMapsViewRenderSystem::OnDirectionalLightAdded>(this);
	sceneRegistry.on_destroy<DirectionalLightData>().connect<&CascadedShadowMapsViewRenderSystem::OnDirectionalLightRemoved>(this);
}

void CascadedShadowMapsViewRenderSystem::OnDeinitialize(RenderView& inRenderView)
{
	Super::OnDeinitialize(inRenderView);
	
	RenderScene& owningScene = GetOwningView().GetRenderScene();
	RenderSceneRegistry& sceneRegistry = owningScene.GetRegistry();

	sceneRegistry.on_construct<DirectionalLightData>().disconnect<&CascadedShadowMapsViewRenderSystem::OnDirectionalLightAdded>(this);
	sceneRegistry.on_update<DirectionalLightData>().disconnect<&CascadedShadowMapsViewRenderSystem::OnDirectionalLightRemoved>(this);
}

void CascadedShadowMapsViewRenderSystem::OnDirectionalLightAdded(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	m_dirLightShadowCascades.emplace_back(CreateDirLightShadowCascades(GetOwningView().GetRenderScene(), RenderSceneEntityHandle(registry, entity)));
}

void CascadedShadowMapsViewRenderSystem::OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	lib::RemoveAllBy(m_dirLightShadowCascades,
					 [entity](const DirLightShadowCascades& cascades)
					 {
						 return cascades.dirLightEntity.entity() == entity;
					 });
}

CascadedShadowMapsViewRenderSystem::DirLightShadowCascades CascadedShadowMapsViewRenderSystem::CreateDirLightShadowCascades(RenderScene& owningScene, RenderSceneEntityHandle lightEntity) const
{
	DirLightShadowCascades shadowCascades;
	shadowCascades.dirLightEntity = lightEntity;

	const SizeType cascadesNum = m_params.cascadesRanges.size();
	shadowCascades.shadowCascadeViews.reserve(cascadesNum);
	shadowCascades.cascadesShadowMaps.reserve(cascadesNum);
	for (Uint32 cascadeIndex = 0; cascadeIndex < cascadesNum; ++cascadeIndex)
	{
		const auto [renderView, shadowMapTexture] = CreateShadowCascadeView(owningScene, lightEntity, cascadeIndex);
		ShadowMap shadowMap;
		shadowMap.textureView                   = shadowMapTexture->CreateView(RENDERER_RESOURCE_NAME("Cascaded Shadow Map View"));
		shadowMap.viewData.viewProjectionMatrix = renderView->GenerateViewProjectionMatrix();
		shadowCascades.shadowCascadeViews.emplace_back(std::move(renderView));
		shadowCascades.cascadesShadowMaps.emplace_back(std::move(shadowMap));
	}

	return shadowCascades;
}

std::pair<lib::SharedRef<RenderView>, lib::SharedRef<rdr::Texture>> CascadedShadowMapsViewRenderSystem::CreateShadowCascadeView(RenderScene& owningScene, RenderSceneEntityHandle lightEntity, Uint32 cascadeIdx) const
{
	SPT_PROFILER_FUNCTION();

	lib::SharedRef<RenderView> renderView = lib::MakeShared<RenderView>(owningScene);
	renderView->AddRenderStages(ERenderStage::ShadowMapRendererStages);

	renderView->SetRenderingRes(m_params.cascadesResolution);

	const rhi::TextureDefinition shadowMapTextureDefinition = ShadowMapUtils::CreateShadowMapDefinition(m_params.cascadesResolution, m_params.shadowsTechnique);
	const lib::SharedRef<rdr::Texture> shadowMap = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Shadow Map Cascade"), shadowMapTextureDefinition, rhi::EMemoryUsage::GPUOnly);

	ShadowMapViewComponent shadowMapViewComponent;
	shadowMapViewComponent.shadowMap         = shadowMap;
	shadowMapViewComponent.owningLight       = lightEntity;
	shadowMapViewComponent.shadowMapType     = EShadowMapType::DirectionalLightCascade;
	shadowMapViewComponent.faceIdx           = 0u;
	shadowMapViewComponent.techniqueOverride = m_params.shadowsTechnique;

	RenderSceneEntityHandle viewEntity = renderView->GetViewEntity();
	viewEntity.emplace<ShadowMapViewComponent>(shadowMapViewComponent);

	return std::make_pair(renderView, shadowMap);
}

CascadedShadowMapsViewRenderSystem::CascadeViewsUpdateParams CascadedShadowMapsViewRenderSystem::ComputeCascadeViewsUpdateParams(const RenderView& owningView) const
{
	SPT_PROFILER_FUNCTION();

	CascadeViewsUpdateParams updateParams;

	const SizeType cascadesNum = m_params.cascadesRanges.size();

	lib::DynamicArray<Real32> cascadeRanges;
	cascadeRanges.reserve(cascadesNum + 1u);
	cascadeRanges.emplace_back(owningView.GetNearPlane() + 0.001f);
	std::copy(std::cbegin(m_params.cascadesRanges), std::cend(m_params.cascadesRanges), std::back_inserter(cascadeRanges));

	updateParams.cascadePlaneEdgePoints.resize(cascadesNum + 1u);

	const math::Matrix4f invViewProj = owningView.GenerateInverseViewProjectionMatrix();

	const lib::StaticArray<math::Vector2f, 4u> edgePoints = {
		math::Vector2f(-1.f, -1.f),
		math::Vector2f(-1.f,  1.f),
		math::Vector2f( 1.f,  1.f),
		math::Vector2f( 1.f, -1.f)
	};

	for (SizeType cascadePlaneIdx = 0u; cascadePlaneIdx < updateParams.cascadePlaneEdgePoints.size(); ++cascadePlaneIdx)
	{
		const Real32 planeProjectedDepth = owningView.ComputeProjectedDepth(cascadeRanges[cascadePlaneIdx]);
		for (SizeType edgePointIdx = 0u; edgePointIdx < edgePoints.size(); ++edgePointIdx)
		{
			const math::Vector4f ndcLocation = math::Vector4f(edgePoints[edgePointIdx].x(), edgePoints[edgePointIdx].y(), planeProjectedDepth, 1.f);
			const math::Vector4f worldSpaceLocation = invViewProj * ndcLocation;
			updateParams.cascadePlaneEdgePoints[cascadePlaneIdx][edgePointIdx] = worldSpaceLocation.head<3>() / worldSpaceLocation.w();
		}
	}

	const auto computeCascadeCenter = [&](SizeType cascadeIdx) -> math::Vector3f
	{
		const CascadeViewsUpdateParams::CascadePlaneEdgePoints& cascadeNearPlaneEdges = updateParams.cascadePlaneEdgePoints[cascadeIdx];
		math::Vector3f sum = math::Vector3f::Zero();
		sum = std::accumulate(std::begin(cascadeNearPlaneEdges), std::end(cascadeNearPlaneEdges), sum);
		const CascadeViewsUpdateParams::CascadePlaneEdgePoints& cascadeFarPlaneEdges = updateParams.cascadePlaneEdgePoints[cascadeIdx + 1u];
		sum = std::accumulate(std::begin(cascadeFarPlaneEdges), std::end(cascadeFarPlaneEdges), sum);
		return 0.125f * sum;
	};

	updateParams.cascadeCenterLocations.resize(cascadesNum);
	for (SizeType cascadeIdx = 0u; cascadeIdx < cascadesNum; ++cascadeIdx)
	{
		updateParams.cascadeCenterLocations[cascadeIdx] = computeCascadeCenter(cascadeIdx);
	}

	return updateParams;
}

void CascadedShadowMapsViewRenderSystem::UpdateCascadeViewsMatrices(const RenderView& owningView, DirLightShadowCascades& cascades, const CascadeViewsUpdateParams& updateParams)
{
	SPT_PROFILER_FUNCTION();

	const SizeType cascadesNum = cascades.shadowCascadeViews.size();
	SPT_CHECK(cascadesNum == m_params.cascadesRanges.size());

	const math::Vector3f lightDirection = cascades.dirLightEntity.get<DirectionalLightData>().direction;

	const Real32 cascadeRenderDistance = 70.f;
	const Real32 cascadeNear = 0.03f;
	const Real32 cascadeFar = cascadeRenderDistance + 50.f;

	for (SizeType cascadeIdx = 0u; cascadeIdx < cascadesNum; ++cascadeIdx)
	{
		RenderView& cascadeRenderView = *cascades.shadowCascadeViews[cascadeIdx];
		const math::Vector3f cascadeRenderLocation = updateParams.cascadeCenterLocations[cascadeIdx] - lightDirection * cascadeRenderDistance;

		cascadeRenderView.SetLocation(cascadeRenderLocation);
		cascadeRenderView.SetRotation(lightDirection);

		const math::Matrix4f lightViewMatrix = cascadeRenderView.GenerateViewMatrix();

		Real32 cascadeLeft   = maxValue<Real32>;
		Real32 cascadeRight  = minValue<Real32>;
		Real32 cascadeBottom = maxValue<Real32>;
		Real32 cascadeUp     = minValue<Real32>;

		const auto computeCascadeProjectionParams = [&](const CascadeViewsUpdateParams::CascadePlaneEdgePoints& edgePoints)
		{
			for (SizeType edgePointIdx = 0u; edgePointIdx < edgePoints.size(); ++edgePointIdx)
			{
				math::Vector4f edgePointLightSpace = lightViewMatrix * edgePoints[edgePointIdx].homogeneous();
				edgePointLightSpace.head<3>() /= edgePointLightSpace.w();

				cascadeLeft   = std::min(cascadeLeft,   -edgePointLightSpace.y());
				cascadeRight  = std::max(cascadeRight,   edgePointLightSpace.y());
				cascadeBottom = std::min(cascadeBottom, -edgePointLightSpace.z());
				cascadeUp     = std::max(cascadeUp,      edgePointLightSpace.z());
			}
		};

		computeCascadeProjectionParams(updateParams.cascadePlaneEdgePoints[cascadeIdx]);
		computeCascadeProjectionParams(updateParams.cascadePlaneEdgePoints[cascadeIdx + 1u]);

		cascadeRenderView.SetOrthographicsProjection(cascadeNear, cascadeFar, cascadeBottom, cascadeUp, cascadeLeft, cascadeRight);
		cascades.cascadesShadowMaps[cascadeIdx].viewData.viewProjectionMatrix = cascadeRenderView.GenerateViewProjectionMatrix();
	}
}

void CascadedShadowMapsViewRenderSystem::CollectCascadeViews(const DirLightShadowCascades& cascades, INOUT RenderViewsCollector& viewsCollector)
{
	for (const auto& view : cascades.shadowCascadeViews)
	{
		viewsCollector.AddRenderView(*view);
	}
}

} // spt::rsc
