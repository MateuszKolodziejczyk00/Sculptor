#pragma once

#include "SceneRenderSystems/SceneRenderSystem.h"
#include "ShaderStructs/ShaderStructs.h"
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
	SHADER_STRUCT_FIELD(HeightFogParams,                              heightFog)
	SHADER_STRUCT_FIELD(Uint32,                                       localLightsNum)
	SHADER_STRUCT_FIELD(Uint32,                                       directionalLightsNum)
	SHADER_STRUCT_FIELD(Bool,                                         hasValidCloudsTransmittanceMap)
	SHADER_STRUCT_FIELD(math::Matrix4f,                               cloudsTransmittanceViewProj)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<LocalLightGPUData>,       localLights)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<DirectionalLightGPUData>, directionalLights)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,         brdfIntegrationLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                    cloudsTransmittanceMap) // only for dir light 0
END_SHADER_STRUCT();


class RENDER_SCENE_API LightsRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	static constexpr ESceneRenderSystem systemType = ESceneRenderSystem::LightsSystem;

	explicit LightsRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	void CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector);
	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings);
	// End SceneRenderSystem overrides

	const rdr::GPUPtr<GlobalLightsParams>& GetGlobalLightsParams() const { return m_globalLightsParams; }

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void BuildLightsTiles(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context);

	void CacheGlobalLightsDS(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context);

	lib::SharedPtr<rdr::Buffer> m_spotLightProxyVertices;
	lib::SharedPtr<rdr::Buffer> m_pointLightProxyVertices;

	lib::SharedPtr<rdr::Buffer> m_lightsDrawCommandsBuffer;

	rdr::GPUPtr<GlobalLightsParams> m_globalLightsParams;
};

} // spt::rsc
