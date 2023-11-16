#pragma once

#include "ViewRenderSystem.h"
#include "ShadowsRenderingTypes.h"


namespace spt::rsc
{

struct ShadowCascadesParams
{
	EShadowMappingTechnique shadowsTechnique = EShadowMappingTechnique::None;
	lib::DynamicArray<Real32> cascadesRanges = { 5.f, 15.f, 40.f };
	math::Vector2u cascadesResolution        = math::Vector2u::Constant(1024);
};


class RENDER_SCENE_API CascadedShadowMapsViewRenderSystem : public ViewRenderSystem
{
protected:

	using Super = ViewRenderSystem;

public:

	explicit CascadedShadowMapsViewRenderSystem(const ShadowCascadesParams& params);

	// Begin ViewRenderSystem interface
	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& owningView, INOUT RenderViewsCollector& viewsCollector) override;
	// End ViewRenderSystem interface

	const lib::DynamicArray<ShadowMap>& GetShadowCascadesViews(RenderSceneEntity directionalLightEntity) const;

private:

	struct DirLightShadowCascades
	{
		RenderSceneEntityHandle                       dirLightEntity;
		lib::DynamicArray<ShadowMap>                  cascadesShadowMaps;
		lib::DynamicArray<lib::SharedRef<RenderView>> shadowCascadeViews;
	};
	
	// Begin ViewRenderSystem interface
	virtual void OnInitialize(RenderView& inRenderView) override;
	virtual void OnDeinitialize(RenderView& inRenderView) override;
	// End ViewRenderSystem interface

	void OnDirectionalLightAdded(RenderSceneRegistry& registry, RenderSceneEntity entity);
	void OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity);

	DirLightShadowCascades CreateDirLightShadowCascades(RenderScene& owningScene, RenderSceneEntityHandle lightEntity) const;

	std::pair<lib::SharedRef<RenderView>, lib::SharedRef<rdr::Texture>> CreateShadowCascadeView(RenderScene& owningScene, RenderSceneEntityHandle lightEntity, Uint32 cascadeIdx) const;

	struct CascadeViewsUpdateParams
	{
		using CascadePlaneEdgePoints = lib::StaticArray<math::Vector3f, 4u>;

		lib::DynamicArray<CascadePlaneEdgePoints> cascadePlaneEdgePoints;
		lib::DynamicArray<math::Vector3f>         cascadeCenterLocations;
	};

	CascadeViewsUpdateParams ComputeCascadeViewsUpdateParams(const RenderView& owningView) const;

	void UpdateCascadeViewsMatrices(const RenderView& owningView, DirLightShadowCascades& cascades, const CascadeViewsUpdateParams& updateParams);
	void CollectCascadeViews(const DirLightShadowCascades& cascades, INOUT RenderViewsCollector& viewsCollector);

	ShadowCascadesParams m_params;

	lib::DynamicArray<DirLightShadowCascades> m_dirLightShadowCascades;
};

} // spt::rsc
