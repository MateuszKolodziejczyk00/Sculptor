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
#include "DescriptorSetBindings/ConstantBufferBinding.h"


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
	rhi::SamplerDefinition definition(rhi::ESamplerFilterType::Linear, rhi::EMipMapAddressingMode::Nearest, rhi::EAxisAddressingMode::ClampToEdge);
	definition.compareOp = rhi::ECompareOp::Greater;
	return definition;
}


BEGIN_SHADER_STRUCT(ShadowsSettings)
	SHADER_STRUCT_FIELD(Uint32, highQualitySMEndIdx)
	SHADER_STRUCT_FIELD(Uint32, mediumQualitySMEndIdx)
	SHADER_STRUCT_FIELD(math::Vector2f, highQualitySMPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, mediumQualitySMPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, lowQualitySMPixelSize)
	SHADER_STRUCT_FIELD(Real32, shadowViewsNearPlane)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(ShadowMapViewData)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewProjectionMatrix)
END_SHADER_STRUCT();


DS_BEGIN(ShadowMapsDS, rg::RGDescriptorSetState<ShadowMapsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<ShadowMapViewData>),						u_shadowMapViews)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMaxClampToEdge>),	u_occludersSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<CreateShadowsSamplerDef()>),				u_shadowSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_shadowMaskSampler)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<256, true>),							u_shadowMaps)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<ShadowsSettings>),					u_shadowsSettings)
DS_END();


struct ShadowMapsRenderingData
{
	lib::SharedPtr<ShadowMapsDS> shadowMapsDS;
};


struct DirectionalLightShadowsData
{
	DirectionalLightShadowsData() = default;

	struct ShadowMask
	{
		lib::SharedPtr<rdr::TextureView> shadowMaskView;
		Uint32 shadowMaskIdx = idxNone<Uint32>;
	};

	void AdvanceFrame()
	{
		currentFrameShadowMaskIdx = currentFrameShadowMaskIdx == 0 ? 1 : 0;
	}

	ShadowMask& GetCurrentFrameShadowMask() {
		return shadowMasks[currentFrameShadowMaskIdx];
	}

	const ShadowMask& GetCurrentFrameShadowMask() const
	{
		return shadowMasks[currentFrameShadowMaskIdx];
	}

	const ShadowMask& GetPreviousFrameShadowMask() const
	{
		return shadowMasks[currentFrameShadowMaskIdx == 0 ? 1 : 0];
	}

	ShadowMask shadowMasks[2];

	Uint32 currentFrameShadowMaskIdx;
};


class RENDER_SCENE_API ShadowMapsManagerSystem : public PrimitivesSystem
{
protected:

	using Super = PrimitivesSystem;

public:

	explicit ShadowMapsManagerSystem(RenderScene& owningScene, const lib::SharedPtr<RenderView>& inMainView);

	ShadowMapsManagerSystem(const ShadowMapsManagerSystem& rhs) = delete;
	ShadowMapsManagerSystem& operator=(const ShadowMapsManagerSystem& rhs) = delete;

	// Begin PrimitivesSystem overrides
	virtual void Update() override;
	// End PrimitivesSystem overrides

	Bool CanRenderShadows() const;

	const lib::SharedPtr<ShadowMapsDS>& GetShadowMapsDS() const;

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

	void UpdateShadowMaps();

	void ReleaseAllShadowMaps();

	void CreateShadowMapsDescriptorSet();
	void UpdateShadowMapsDSViewsData();

	Real32 ComputeLocalLightShadowMapPriority(const SceneView& view, RenderSceneEntity light) const;

	// Directional lights ========================================================

	void UpdateDirectionalLightShadowMaps() const;

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

	lib::DynamicArray<RenderSceneEntity> m_lightsWithUpdatedShadowMaps;

	lib::DynamicArray<ShadowMapViewData> m_shadowMapViewsData;
	lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> m_shadowMapViewsBuffers;

	lib::SharedPtr<ShadowMapsDS> m_shadowMapsDS;

	lib::WeakPtr<RenderView> m_mainView;
};

} // spt::rsc