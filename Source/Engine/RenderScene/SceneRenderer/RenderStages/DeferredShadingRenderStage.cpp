#include "DeferredShadingRenderStage.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "RenderScene.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "Lights/ViewShadingInput.h"
#include "GlobalResources/GlobalResources.h"
#include "DDGI/DDGISceneSubsystem.h"
#include "ParticipatingMedia/ParticipatingMediaTypes.h"
#include "ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::DeferredShading, DeferredShadingRenderStage);


namespace deferred_shading
{

BEGIN_SHADER_STRUCT(DeferredShadingContstants)
	SHADER_STRUCT_FIELD(math::Vector2u,  resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,  pixelSize)
	SHADER_STRUCT_FIELD(Bool,            isAmbientOcclusionEnabled)
END_SHADER_STRUCT();


DS_BEGIN(DeferredShadingDS, rg::RGDescriptorSetState<DeferredShadingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                      u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                      u_blueNoise256Texture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),              u_gBuffer0Texture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),              u_gBuffer1Texture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                      u_gBuffer2Texture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),              u_gBuffer3Texture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                      u_gBuffer4Texture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),               u_luminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DeferredShadingContstants>), u_deferredShadingConstants)
DS_END();


static rdr::PipelineStateID CompileDeferredShadingPipeline(Bool isDDGIEnabled)
{
	sc::ShaderCompilationSettings shaderCompilationSettings;
	if(isDDGIEnabled)
	{
		shaderCompilationSettings.AddMacroDefinition(sc::MacroDefinition("ENABLE_DDGI", "1"));
	}

	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Shading/DeferredShading.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DeferredShadingCS"), shaderCompilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DeferredShadingPipeline"), shader);
}


struct DeferredShadingParams
{
	ViewRenderingSpec& viewSpec;
	const RenderScene& renderScene;
};


void ExecuteDeferredShading(rg::RenderGraphBuilder& graphBuilder, const DeferredShadingParams& shadingParams)
{
	SPT_PROFILER_FUNCTION();

	ViewRenderingSpec& viewSpec = shadingParams.viewSpec;

	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	const GBuffer& gBuffer = viewContext.gBuffer;

	viewContext.luminance = graphBuilder.CreateTextureView(RG_DEBUG_NAME("View Luminance Texture"), rg::TextureDef(resolution, SceneRendererStatics::hdrFormat));

	DeferredShadingContstants shaderConstants;
	shaderConstants.resolution                = resolution;
	shaderConstants.pixelSize                 = math::Vector2f::Ones().cwiseQuotient(resolution.cast<Real32>());
	shaderConstants.isAmbientOcclusionEnabled = viewContext.ambientOcclusion.IsValid();

	lib::MTHandle<DeferredShadingDS> deferredShadingDS = graphBuilder.CreateDescriptorSet<DeferredShadingDS>(RENDERER_RESOURCE_NAME("DeferredShadingDS"));
	deferredShadingDS->u_depthTexture             = viewContext.depth;
	deferredShadingDS->u_blueNoise256Texture      = gfx::global::Resources::Get().blueNoise256.GetView();
	deferredShadingDS->u_gBuffer0Texture          = gBuffer[0];
	deferredShadingDS->u_gBuffer1Texture          = gBuffer[1];
	deferredShadingDS->u_gBuffer2Texture          = gBuffer[2];
	deferredShadingDS->u_gBuffer3Texture          = gBuffer[3];
	deferredShadingDS->u_gBuffer4Texture          = gBuffer[4];
	deferredShadingDS->u_luminanceTexture         = viewContext.luminance;
	deferredShadingDS->u_deferredShadingConstants = shaderConstants;

	const Bool isDDGIEnabled = false;
	const rdr::PipelineStateID pipeline = CompileDeferredShadingPipeline(isDDGIEnabled);

	lib::MTHandle<ddgi::DDGISceneDS> ddgiDS;
	if (isDDGIEnabled)
	{
		const ddgi::DDGISceneSubsystem& ddgiSceneSubsystem = shadingParams.renderScene.GetSceneSubsystemChecked<ddgi::DDGISceneSubsystem>();
		ddgiDS = ddgiSceneSubsystem.GetDDGISceneDS();
	}

	graphBuilder.Dispatch(RG_DEBUG_NAME("Deferred Shading"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u{ 8u, 8u }),
						  rg::BindDescriptorSets(std::move(deferredShadingDS),
												 viewSpec.GetRenderView().GetRenderViewDS(),
												 viewContext.shadowMapsDS,
												 viewContext.shadingInputDS,
												 ddgiDS));
}

} // deferred_shading

DeferredShadingRenderStage::DeferredShadingRenderStage()
{
}

void DeferredShadingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	deferred_shading::DeferredShadingParams params{ viewSpec, renderScene };

	deferred_shading::ExecuteDeferredShading(graphBuilder, params);
}

} // spt::rsc
