#include "VolumetricFogRenderStage.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "View/RenderView.h"
#include "ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::VolumetricFog, VolumetricFogRenderStage);

namespace apply_volumetric_fog
{

BEGIN_SHADER_STRUCT(ApplyVolumetricFogParams)
	SHADER_STRUCT_FIELD(Real32, fogNearPlane)
	SHADER_STRUCT_FIELD(Real32, fogFarPlane)
	SHADER_STRUCT_FIELD(Real32, blendPixelsOffset)
END_SHADER_STRUCT();


DS_BEGIN(ApplyVolumetricFogDS, rg::RGDescriptorSetState<ApplyVolumetricFogDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),                            u_integratedInScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),  u_integratedInScatteringSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_luminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ApplyVolumetricFogParams>),                u_applyVolumetricFogParams)
DS_END();


static rdr::PipelineStateID CompileApplyVolumetricFogPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ApplyVolumetricFog.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ApplyVolumetricFogCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ApplyVolumetricFogPipeline"), shader);
}


static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	ApplyVolumetricFogParams params;
	params.fogNearPlane			= fogParams.nearPlane;
	params.fogFarPlane			= fogParams.farPlane;
	params.blendPixelsOffset	= 8.f;

	const lib::MTHandle<ApplyVolumetricFogDS> applyVolumetricFogDS = graphBuilder.CreateDescriptorSet<ApplyVolumetricFogDS>(RENDERER_RESOURCE_NAME("ApplyVolumetricFogDS"));
	applyVolumetricFogDS->u_integratedInScatteringTexture = fogParams.integratedInScatteringTextureView;
	applyVolumetricFogDS->u_luminanceTexture              = viewContext.luminance;
	applyVolumetricFogDS->u_depthTexture                  = viewContext.depth;
	applyVolumetricFogDS->u_applyVolumetricFogParams      = params;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(renderView.GetRenderingRes3D(), math::Vector3u(8u, 8u, 1u));

	static const rdr::PipelineStateID applyVolumetricFogPipeline = CompileApplyVolumetricFogPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Volumetric Fog"),
						  applyVolumetricFogPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(applyVolumetricFogDS, renderView.GetRenderViewDS()));
}

} // apply_volumetric_fog

VolumetricFogRenderStage::VolumetricFogRenderStage()
{ }

void VolumetricFogRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	ParticipatingMediaViewRenderSystem& participatingMediaViewSystem = renderView.GetRenderSystem<ParticipatingMediaViewRenderSystem>();

	apply_volumetric_fog::Render(graphBuilder, renderScene, viewSpec, participatingMediaViewSystem.GetVolumetricFogParams());

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
