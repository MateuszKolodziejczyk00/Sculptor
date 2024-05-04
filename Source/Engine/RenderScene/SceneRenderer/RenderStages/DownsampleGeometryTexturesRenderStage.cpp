#include "DownsampleGeometryTexturesRenderStage.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "Pipelines/PipelineState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "ResourcesManager.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::DownsampleGeometryTextures, DownsampleGeometryTexturesRenderStage);


BEGIN_SHADER_STRUCT(DownsampleGeometryTexturesConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, inputRes)
	SHADER_STRUCT_FIELD(math::Vector2f, inputPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2u, outputRes)
	SHADER_STRUCT_FIELD(math::Vector2f, outputPixelSize)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(DownsampleGeometryTexturesDS, rg::RGDescriptorSetState<DownsampleGeometryTexturesDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_tangentFrameTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_depthTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                             u_motionTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_normalsTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_roughnessTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DownsampleGeometryTexturesConstants>),     u_constants)
DS_END();


static rdr::PipelineStateID CompileDownsampleGeometryTexturesPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/DownsampleGeometryTextures/DownsampleGeometryTextures.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DownsampleGeometryTexturesCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DownsampleGeometryTexturesPipeline"), shader);
}


DownsampleGeometryTexturesRenderStage::DownsampleGeometryTexturesRenderStage()
{ }

void DownsampleGeometryTexturesRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	SPT_CHECK(viewContext.depth.IsValid());
	SPT_CHECK(viewContext.motion.IsValid());

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u renderingResolution = renderView.GetRenderingRes();

	const math::Vector2u halfRes = math::Utils::DivideCeil(renderingResolution, math::Vector2u(2u, 2u));

	PrepareResources(halfRes);

	SPT_CHECK(viewContext.depthHalfRes.IsValid());
	SPT_CHECK(viewContext.depthHalfRes->GetResolution2D() == halfRes);

	const rhi::EFragmentFormat motionFormat = viewContext.motion->GetFormat();
	viewContext.motionHalfRes = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Motion Texture Half Res"), rg::TextureDef(halfRes, motionFormat));

	viewContext.normalsHalfRes = graphBuilder.AcquireExternalTextureView(m_normalsTextureHalfRes);

	if (m_historyNormalsTextureHalfRes)
	{
		viewContext.historyNormalsHalfRes = graphBuilder.AcquireExternalTextureView(m_historyNormalsTextureHalfRes);
	}

	const rhi::EFragmentFormat roughnessFormat = GBuffer::GetFormat(GBuffer::Texture::Roughness);
	viewContext.roughnessHalfRes = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Roughness Half Res"), rg::TextureDef(halfRes, roughnessFormat));

	// TODO
	static Uint32 testVal = 0;
	testVal++;

	DownsampleGeometryTexturesConstants shaderConstants;
	shaderConstants.inputRes        = renderingResolution;
	shaderConstants.inputPixelSize  = renderingResolution.cast<Real32>().cwiseInverse();
	shaderConstants.outputRes       = halfRes;
	shaderConstants.outputPixelSize = halfRes.cast<Real32>().cwiseInverse();
	shaderConstants.frameIdx = testVal;

	lib::MTHandle<DownsampleGeometryTexturesDS> ds = graphBuilder.CreateDescriptorSet<DownsampleGeometryTexturesDS>(RENDERER_RESOURCE_NAME("Downsample Geometry Textures DS"));
	ds->u_depthTexture            = viewContext.depth;
	ds->u_motionTexture           = viewContext.motion;
	ds->u_tangentFrameTexture     = viewContext.gBuffer[GBuffer::Texture::TangentFrame];
	ds->u_roughnessTexture        = viewContext.gBuffer[GBuffer::Texture::Roughness];
	ds->u_depthTextureHalfRes     = viewContext.depthHalfRes;
	ds->u_motionTextureHalfRes    = viewContext.motionHalfRes;
	ds->u_normalsTextureHalfRes   = viewContext.normalsHalfRes;
	ds->u_roughnessTextureHalfRes = viewContext.roughnessHalfRes;
	ds->u_constants               = shaderConstants;

	const rdr::PipelineStateID pipeline = CompileDownsampleGeometryTexturesPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Downsample Geometry Textures"),
						  pipeline,
						  math::Utils::DivideCeil(halfRes, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), renderView.GetRenderViewDS()));

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

void DownsampleGeometryTexturesRenderStage::PrepareResources(math::Vector2u renderingHalfRes)
{
	SPT_PROFILER_FUNCTION();

	std::swap(m_normalsTextureHalfRes, m_historyNormalsTextureHalfRes);

	if (!m_normalsTextureHalfRes || m_normalsTextureHalfRes->GetResolution2D() != renderingHalfRes)
	{
		rhi::TextureDefinition normalsDef;
		normalsDef.resolution = renderingHalfRes;
		normalsDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource);
		normalsDef.format     = rhi::EFragmentFormat::RGBA16_UN_Float;
		m_normalsTextureHalfRes = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Normals Texture Half Res"), normalsDef, rhi::EMemoryUsage::GPUOnly);
	}
}

} // spt::rsc
