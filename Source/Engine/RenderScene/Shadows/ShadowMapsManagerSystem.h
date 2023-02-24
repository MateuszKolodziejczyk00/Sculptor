#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "PrimitivesSystem.h"
#include "RenderSceneRegistry.h"
#include "View/RenderView.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "RHICore/RHISamplerTypes.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"


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


struct VisibleLightEntityInfo
{
	RenderSceneEntity	entity;
	Real32				areaOnScreen;
};


constexpr rhi::SamplerDefinition CreateShadowsSamplerDef()
{
	rhi::SamplerDefinition definition(rhi::ESamplerFilterType::Linear, rhi::EMipMapAddressingMode::Nearest, rhi::EAxisAddressingMode::ClampToBorder);
	definition.compareOp = rhi::ECompareOp::Less;
	return definition;
}


BEGIN_SHADER_STRUCT(ShadowMapViewData)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewProjectionMatrix)
END_SHADER_STRUCT();

DS_BEGIN(ShadowMapsDS, rg::RGDescriptorSetState<ShadowMapsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<ShadowMapViewData>),			u_shadowMapViews)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<CreateShadowsSamplerDef()>),	u_shadowSampler)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<256, true>),				u_shadowMaps)
DS_END();


struct ShadowMapsRenderingData
{
	lib::SharedPtr<ShadowMapsDS> shadowMapsDS;
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

	const lib::SharedPtr<ShadowMapsDS>& GetShadowMapsDS() const;
	
	void UpdateVisibleLights(lib::DynamicArray<VisibleLightEntityInfo>& visibleLights);

	const lib::DynamicArray<RenderSceneEntity>& GetPointLightsWithShadowMapsToUpdate() const;
	
	lib::DynamicArray<RenderView*> GetShadowMapViewsToUpdate() const;

	lib::DynamicArray<RenderView*> GetPointLightShadowMapViews(const PointLightShadowMapComponent& pointLightShadowMap) const;

private:

	void SetPointLightShadowMapsBeginIdx(RenderSceneEntity pointLightEntity, Uint32 shadowMapBeginIdx);
	Uint32 ResetPointLightShadowMap(RenderSceneEntity pointLightEntity);

	EShadowMapQuality GetShadowMapQuality(SizeType pointLightIdx) const;

	void ReleaseShadowMap(EShadowMapQuality quality, Uint32 shadowMapIdx);
	Uint32 AcquireAvaialableShadowMap(EShadowMapQuality quality);

	void CreateShadowMaps();
	void CreateShadowMapsRenderViews();

	void UpdateShadowMapRenderViews(RenderSceneEntity owningLight, const PointLightData& pointLight, Uint32 shadowMapBeginIdx);

	void UpdateShadowMaps();

	void CreateShadowMapsDescriptorSet();
	void UpdateShadowMapsDSViewsData();

	lib::DynamicArray<lib::SharedRef<rdr::Texture>> m_shadowMaps;
	lib::DynamicArray<lib::UniquePtr<RenderView>> m_shadowMapsRenderViews;
	
	lib::HashMap<EShadowMapQuality, lib::DynamicArray<Uint32>> m_availableShadowMaps;

	Uint32 highQualityShadowMapsMaxIdx;
	Uint32 mediumQualityShadowMapsMaxIdx;

	lib::HashMap<RenderSceneEntity, EShadowMapQuality> m_pointLightsWithAssignedShadowMaps;

	struct LightUpdatePriority
	{
		RenderSceneEntity	light;
		Real32				updatePriority;
	};

	lib::DynamicArray<LightUpdatePriority> m_updatePriorities;

	lib::DynamicArray<RenderSceneEntity> m_lightsWithUpdatedShadowMaps;

	lib::DynamicArray<ShadowMapViewData> m_shadowMapViewsData;
	lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> m_shadowMapViewsBuffers;

	lib::SharedPtr<ShadowMapsDS> m_shadowMapsDS;
};

} // spt::rsc