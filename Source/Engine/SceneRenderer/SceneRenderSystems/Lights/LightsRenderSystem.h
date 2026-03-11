#pragma once

#include "SceneRenderSystems/SceneRenderSystem.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Lights/LightTypes.h"
#include "ViewRenderSystems/ParticipatingMedia/ParticipatingMediaTypes.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::rsc
{

class ShadowMapsDS;


BEGIN_SHADER_STRUCT(GlobalLightsParams)
	SHADER_STRUCT_FIELD(HeightFogParams, heightFog)
	SHADER_STRUCT_FIELD(Uint32,          localLightsNum)
	SHADER_STRUCT_FIELD(Uint32,          directionalLightsNum)
	SHADER_STRUCT_FIELD(Bool,            hasValidCloudsTransmittanceMap)
	SHADER_STRUCT_FIELD(math::Matrix4f,  cloudsTransmittanceViewProj)
END_SHADER_STRUCT();


DS_BEGIN(GlobalLightsDS, rg::RGDescriptorSetState<GlobalLightsDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GlobalLightsParams>),                     u_lightsParams)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<LocalLightGPUData>),                    u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<DirectionalLightGPUData>),              u_directionalLights)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_brdfIntegrationLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_brdfIntegrationLUTSampler)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                           u_cloudsTransmittanceMap) // only for dir light 0
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_cloudsTransmittanceMapSampler)
DS_END();


class RENDER_SCENE_API LightsRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	static constexpr ESceneRenderSystem systemType = ESceneRenderSystem::LightsSystem;

	explicit LightsRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	void CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector);
	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings);
	// End SceneRenderSystem overrides

	const lib::MTHandle<GlobalLightsDS>& GetGlobalLightsDS() const;

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void BuildLightsTiles(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context);

	void CacheGlobalLightsDS(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context);

	lib::SharedPtr<rdr::Buffer> m_spotLightProxyVertices;
	lib::SharedPtr<rdr::Buffer> m_pointLightProxyVertices;

	lib::SharedPtr<rdr::Buffer> m_lightsDrawCommandsBuffer;

	lib::MTHandle<GlobalLightsDS> m_globalLightsDS;
};

} // spt::rsc
