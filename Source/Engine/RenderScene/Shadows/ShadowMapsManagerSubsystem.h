#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneSubsystem.h"
#include "RenderSceneRegistry.h"
#include "View/RenderView.h"
#include "ShaderStructs/ShaderStructs.h"
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
SPT_REGISTER_COMPONENT_TYPE(PointLightShadowMapComponent, RenderSceneRegistry);


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


BEGIN_SHADER_STRUCT(ShadowMapTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, texture)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(ShadowMapsData)
	SHADER_STRUCT_FIELD(ShadowsSettings,                        settings)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<ShadowMapViewData>, shadowMapViews)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<ShadowMapTexture>,  shadowMaps)
END_SHADER_STRUCT();


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
	virtual void UpdateGPUSceneData(RenderSceneConstants& sceneData) override;
	// End PrimitivesSystem overrides

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