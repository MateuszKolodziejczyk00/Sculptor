#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "PrimitivesSystem.h"
#include "RenderSceneRegistry.h"
#include "View/RenderView.h"


namespace spt::rdr
{
class TextureView;
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


struct VisibleLightEntityInfo
{
	RenderSceneEntity	entity;
	Real32				areaOnScreen;
};


class RENDER_SCENE_API ShadowMapsManagerSystem : public PrimitivesSystem
{
protected:

	using Super = PrimitivesSystem;

public:

	explicit ShadowMapsManagerSystem(RenderScene& owningScene);

	ShadowMapsManagerSystem(const ShadowMapsManagerSystem& rhs) = delete;
	ShadowMapsManagerSystem& operator=(const ShadowMapsManagerSystem& rhs) = delete;

	// Begin PrimitivesSystem overrides
	virtual void Update() override;
	// End PrimitivesSystem overrides

	Bool CanRenderShadows() const;
	
	void UpdateVisibleLights(lib::DynamicArray<VisibleLightEntityInfo>& visibleLights);

	const lib::DynamicArray<RenderSceneEntity>& GetLightsWithShadowMapsToUpdate() const;
	
	lib::DynamicArray<RenderView*> GetShadowMapViewsToUpdate() const;

private:

	void SetPointLightShadowMapsBeginIdx(RenderSceneEntity pointLightEntity, Uint32 shadowMapBeginIdx);
	Uint32 ResetPointLightShadowMap(RenderSceneEntity pointLightEntity) const;

	EShadowMapQuality GetShadowMapQuality(SizeType pointLightIdx) const;

	void ReleaseShadowMap(EShadowMapQuality quality, Uint32 shadowMapIdx);
	Uint32 AcquireAvaialableShadowMap(EShadowMapQuality quality);

	void CreateShadowMaps();
	void CreateShadowMapsRenderViews();

	void UpdateShadowMapRenderViews(RenderSceneEntity owningLight, const PointLightData& pointLight, Uint32 shadowMapBeginIdx);

	lib::DynamicArray<lib::SharedRef<rdr::TextureView>> m_shadowMapViews;
	lib::DynamicArray<lib::UniquePtr<RenderView>> m_shadowMapsRenderViews;

	lib::HashMap<RenderSceneEntity, EShadowMapQuality> m_pointLightsWithAssignedShadowMaps;
	
	lib::HashMap<EShadowMapQuality, lib::DynamicArray<Uint32>> m_availableShadowMaps;

	Uint32 highQualityShadowMapsMaxIdx;
	Uint32 mediumQualityShadowMapsMaxIdx;

	lib::DynamicArray<RenderSceneEntity> m_lightsWithUpdatedShadowMaps;
};

} // spt::rsc