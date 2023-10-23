#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneSubsystem.h"
#include "RenderSceneRegistry.h"
#include "View/RenderView.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "RHICore/RHISamplerTypes.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
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


struct VisibleLightEntityInfo
{
	RenderSceneEntity	entity;
	Real32				areaOnScreen;
};


BEGIN_SHADER_STRUCT(ShadowsSettings)
	SHADER_STRUCT_FIELD(Uint32, highQualitySMEndIdx)
	SHADER_STRUCT_FIELD(Uint32, mediumQualitySMEndIdx)
	SHADER_STRUCT_FIELD(math::Vector2f, highQualitySMPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, mediumQualitySMPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, lowQualitySMPixelSize)
	SHADER_STRUCT_FIELD(Real32, shadowViewsNearPlane)
	SHADER_STRUCT_FIELD(Uint32, shadowMappingTechnique)
END_SHADER_STRUCT();


DS_BEGIN(ShadowMapsDS, rg::RGDescriptorSetState<ShadowMapsDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<ShadowMapViewData>),				u_shadowMapViews)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMaxClampToEdge>),	u_shadowMapSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearShadowMapSampler)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<256, true>),							u_shadowMaps)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<ShadowsSettings>),					u_shadowsSettings)
DS_END();


class RENDER_SCENE_API ShadowMapsManagerSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit ShadowMapsManagerSubsystem(RenderScene& owningScene, const lib::SharedPtr<RenderView>& inMainView);

	ShadowMapsManagerSubsystem(const ShadowMapsManagerSubsystem& rhs) = delete;
	ShadowMapsManagerSubsystem& operator=(const ShadowMapsManagerSubsystem& rhs) = delete;

	// Begin PrimitivesSystem overrides
	virtual void Update() override;
	// End PrimitivesSystem overrides

	void SetShadowMappingTechnique(EShadowMappingTechnique newTechnique);
	EShadowMappingTechnique GetShadowMappingTechnique() const;

	void UpdateVisibleLocalLights(const lib::SharedPtr<rdr::Buffer>& visibleLightsBuffer);

	/** Returns true only for view that is detemining which lights should cast shadows */
	Bool IsMainView(const RenderView& renderView) const;

	Bool CanRenderShadows() const;

	const lib::MTHandle<ShadowMapsDS>& GetShadowMapsDS() const;

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

	void CreateShadowMapsDescriptorSet();
	void UpdateShadowMapsDSViewsData();

	Bool IsLocalLightVisible(RenderSceneEntity light) const;

	Real32 ComputeLocalLightShadowMapPriority(const SceneView& view, RenderSceneEntity light) const;

	void RecreateShadowMaps();

	lib::DynamicArray<lib::SharedRef<rdr::Texture>> m_shadowMaps;
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
	lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> m_shadowMapViewsBuffers;

	lib::MTHandle<ShadowMapsDS> m_shadowMapsDS;

	lib::WeakPtr<RenderView> m_mainView;

	lib::HashSet<RenderSceneEntity> m_visibleLocalLightsSet;

	EShadowMappingTechnique m_shadowMapTechnique;
};

} // spt::rsc