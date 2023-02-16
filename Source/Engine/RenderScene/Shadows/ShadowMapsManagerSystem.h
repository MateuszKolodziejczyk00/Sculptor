#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "PrimitivesSystem.h"
#include "RenderSceneRegistry.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

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
	Uint32 shadowMapIdxBegin;
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

	Bool CanRenderShadows() const;
	
	void UpdateVisibleLights(lib::DynamicArray<VisibleLightEntityInfo>& visibleLights);

private:

	void SetPointLightShadowMapsBeginIdx(RenderSceneEntity pointLightEntity, Uint32 shadowMapBeginIdx) const;
	Uint32 ResetPointLightShadowMap(RenderSceneEntity pointLightEntity) const;

	EShadowMapQuality GetShadowMapQuality(SizeType pointLightIdx) const;

	void ReleaseShadowMap(EShadowMapQuality quality, Uint32 shadowMapIdx);
	Uint32 AcquireAvaialableShadowMap(EShadowMapQuality quality);

	void CreateShadowMaps();

	lib::DynamicArray<lib::SharedRef<rdr::TextureView>> m_shadowMapViews;

	lib::HashMap<RenderSceneEntity, EShadowMapQuality> m_pointLightsWithAssignedShadowMaps;
	
	lib::HashMap<EShadowMapQuality, lib::DynamicArray<Uint32>> m_availableShadowMaps;
};

} // spt::rsc