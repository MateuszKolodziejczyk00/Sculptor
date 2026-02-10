#pragma once

#include "RenderSceneMacros.h"
#include "SceneRenderSystem.h"
#include "RenderSceneRegistry.h"
#include "View/RenderView.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ShadowsRenderingTypes.h"


namespace spt::rdr
{
class Texture;
} // spt::rdr


namespace spt::rsc
{

struct PointLightData;


enum class EShadowMapQuality
{
	None,
	Low,
	Medium,
	High,
};


struct PointLightShadowMapComponent
{
	// Idx of first shadow map view used for this point lights
	// Shadow maps for different sides of point lights must be continuous
	Uint32 shadowMapFirstFaceIdx;
};
SPT_REGISTER_COMPONENT_TYPE(PointLightShadowMapComponent, RenderSceneRegistry);


struct VisibleLightEntityInfo
{
	RenderSceneEntity	entity;
	Real32				areaOnScreen;
};


class RENDER_SCENE_API ShadowMapsRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	explicit ShadowMapsRenderSystem(RenderScene& owningScene, const lib::SharedPtr<RenderView>& inMainView);

	ShadowMapsRenderSystem(const ShadowMapsRenderSystem& rhs) = delete;
	ShadowMapsRenderSystem& operator=(const ShadowMapsRenderSystem& rhs) = delete;

	// Begin SceneRenderSystem overrides
	virtual void Update() override;
	virtual void UpdateGPUSceneData(RenderSceneConstants& sceneData) override;
	// End SceneRenderSystem overrides

	void SetShadowMappingTechnique(EShadowMappingTechnique newTechnique);
	EShadowMappingTechnique GetShadowMappingTechnique() const;

	/** Returns true only for view that is detemining which lights should cast shadows */
	Bool IsMainView(const RenderView& renderView) const;

	Bool CanRenderShadows() const;

	const lib::DynamicArray<RenderSceneEntity>& GetPointLightsWithShadowMapsToUpdate() const;
	
	lib::DynamicArray<RenderView*> GetShadowMapViewsToUpdate() const;

	lib::DynamicArray<RenderView*> GetPointLightShadowMapViews(const PointLightShadowMapComponent& pointLightShadowMap) const;

private:

	void SetPointLightShadowMapsBeginIdx(RenderSceneEntity pointLightEntity, Uint32 shadowMapBeginIdx);
	Uint32 ResetPointLightShadowMap(RenderSceneEntity pointLightEntity);

	EShadowMapQuality GetShadowMapQuality(SizeType pointLightIdx) const;
	EShadowMapQuality GetShadowMapQuality(RenderSceneEntity light) const;

	void ReleaseShadowMap(EShadowMapQuality quality, Uint32 shadowMapIdx);
	Uint32 AcquireAvaialableShadowMap(EShadowMapQuality quality);

	void CreateShadowMaps();
	void CreateShadowMapsRenderViews();
	
	void AssignShadowMaps();

	void UpdateShadowMapRenderViews(RenderSceneEntity owningLight, const PointLightData& pointLight, Uint32 shadowMapBeginIdx);

	void FindShadowMapsToUpdate();

	void ReleaseAllShadowMaps();

	Real32 ComputeLocalLightShadowMapPriority(const SceneView& view, RenderSceneEntity light) const;

	void RecreateShadowMaps();

	lib::DynamicArray<lib::SharedRef<rdr::TextureView>> m_shadowMapViews;
	lib::DynamicArray<lib::UniquePtr<RenderView>> m_shadowMapsRenderViews;
	
	lib::HashMap<EShadowMapQuality, lib::DynamicArray<Uint32>> m_availableShadowMaps;

	Uint32 m_highQualityShadowMapLightEndIdx;
	Uint32 m_mediumQualityShadowMapsLightEndIdx;

	lib::HashMap<RenderSceneEntity, EShadowMapQuality> m_pointLightsWithAssignedShadowMaps;

	struct LightUpdatePriority
	{
		RenderSceneEntity	light;
		Real32				updatePriority;
	};

	lib::DynamicArray<LightUpdatePriority> m_updatePriorities;

	lib::DynamicArray<RenderSceneEntity> m_lightsWithShadowMapsToUpdate;

	lib::DynamicArray<ShadowMapViewData> m_shadowMapViewsData;

	lib::SharedPtr<rdr::Buffer> m_shadowMapTexturesBuffer;
	lib::SharedPtr<rdr::Buffer> m_shadowMapViewsBuffer;

	lib::WeakPtr<RenderView> m_mainView;

	EShadowMappingTechnique m_shadowMapTechnique;
};

} // spt::rsc
