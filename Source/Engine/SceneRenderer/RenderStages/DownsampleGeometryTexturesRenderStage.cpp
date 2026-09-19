#include "DownsampleGeometryTexturesRenderStage.h"
#include "RenderGraphBuilder.h"
#include "Pipelines/PipelineState.h"
#include "ResourcesManager.h"
#include "SceneRenderer/Utils/LinearizeDepth.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::DownsampleGeometryTextures, DownsampleGeometryTexturesRenderStage);


BEGIN_SHADER_STRUCT(DownsampleGeometryTexturesConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                    inputRes)
	SHADER_STRUCT_FIELD(math::Vector2f,                    inputPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2u,                    outputRes)
	SHADER_STRUCT_FIELD(math::Vector2f,                    outputPixelSize)
	SHADER_STRUCT_FIELD(Bool,                              downsampleRoughness)
	SHADER_STRUCT_FIELD(Bool,                              downsampleBaseColor)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, motionTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, tangentFrameTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, baseColorMetallicTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         depthTextureHalfRes)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, motionTextureHalfRes)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, normalsTextureHalfRes)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, baseColorTextureHalfRes)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         roughnessTextureHalfRes)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileDownsampleGeometryTexturesPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/DownsampleGeometryTextures/DownsampleGeometryTextures.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DownsampleGeometryTexturesCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DownsampleGeometryTexturesPipeline"), shader);
}


DownsampleGeometryTexturesRenderStage::DownsampleGeometryTexturesRenderStage()
{ }

void DownsampleGeometryTexturesRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	SPT_CHECK(viewContext.depth.IsValid());
	SPT_CHECK(viewContext.motion.IsValid());

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u renderingResolution = viewSpec.GetRenderingRes();

	const math::Vector2u halfRes = math::Utils::DivideCeil(renderingResolution, math::Vector2u(2u, 2u));

	PrepareResources(viewSpec);

	SPT_CHECK(viewContext.depthHalfRes.IsValid());
	SPT_CHECK(viewContext.depthHalfRes->GetResolution2D() == halfRes);

	const rhi::EFragmentFormat motionFormat = viewContext.motion->GetFormat();
	viewContext.motionHalfRes = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Motion Texture Half Res"), rg::TextureDef(halfRes, motionFormat));

	const ShadingViewResourcesUsageInfo& resourcesUsageInfo = viewSpec.GetBlackboard().GetOrCreate<ShadingViewResourcesUsageInfo>();

	viewContext.normalsHalfRes = graphBuilder.AcquireExternalTextureView(m_normalsTextureHalfRes);
	if (m_historyNormalsTextureHalfRes)
	{
		viewContext.historyNormalsHalfRes = graphBuilder.AcquireExternalTextureView(m_historyNormalsTextureHalfRes);
	}

	if (resourcesUsageInfo.useHalfResRoughnessWithHistory)
	{
		viewContext.roughnessHalfRes = graphBuilder.AcquireExternalTextureView(m_roughnessTextureHalfRes);
		if (m_historyRoughnessTextureHalfRes)
		{
			viewContext.historyRoughnessHalfRes = graphBuilder.AcquireExternalTextureView(m_historyRoughnessTextureHalfRes);
		}
	}

	if (resourcesUsageInfo.useHalfResBaseColorWithHistory)
	{
		viewContext.baseColorHalfRes = graphBuilder.AcquireExternalTextureView(m_baseColorTextureHalfRes);
		if (m_historyBaseColorTextureHalfRes)
		{
			viewContext.historyBaseColorHalfRes = graphBuilder.AcquireExternalTextureView(m_historyBaseColorTextureHalfRes);
		}
	}

	DownsampleGeometryTexturesConstants shaderConstants;
	shaderConstants.inputRes            = renderingResolution;
	shaderConstants.inputPixelSize      = renderingResolution.cast<Real32>().cwiseInverse();
	shaderConstants.outputRes           = halfRes;
	shaderConstants.outputPixelSize     = halfRes.cast<Real32>().cwiseInverse();
	shaderConstants.downsampleRoughness = resourcesUsageInfo.useHalfResRoughnessWithHistory;
	shaderConstants.downsampleBaseColor = resourcesUsageInfo.useHalfResBaseColorWithHistory;
	shaderConstants.depthTexture                = viewContext.depth;
	shaderConstants.motionTexture               = viewContext.motion;
	shaderConstants.tangentFrameTexture         = viewContext.gBuffer[GBuffer::Texture::TangentFrame];
	shaderConstants.roughnessTexture            = viewContext.gBuffer[GBuffer::Texture::Roughness];
	shaderConstants.baseColorMetallicTexture    = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
	shaderConstants.depthTextureHalfRes         = viewContext.depthHalfRes;
	shaderConstants.motionTextureHalfRes        = viewContext.motionHalfRes;
	shaderConstants.normalsTextureHalfRes       = viewContext.normalsHalfRes;

	if (resourcesUsageInfo.useHalfResRoughnessWithHistory)
	{
		shaderConstants.roughnessTextureHalfRes = viewContext.roughnessHalfRes;
	}

	if (resourcesUsageInfo.useHalfResBaseColorWithHistory)
	{
		shaderConstants.baseColorTextureHalfRes = viewContext.baseColorHalfRes;
	}

	static const rdr::PipelineStateID pipeline = CompileDownsampleGeometryTexturesPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Downsample Geometry Textures"),
						  pipeline,
						  math::Utils::DivideCeil(halfRes, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));

	if (resourcesUsageInfo.useLinearDepthHalfRes)
	{
		viewContext.linearDepthHalfRes = ExecuteLinearizeDepth(graphBuilder, renderView, viewContext.depthHalfRes);
	}
}

void DownsampleGeometryTexturesRenderStage::PrepareResources(const ViewRenderingSpec& viewSpec)
{
	const math::Vector2u renderingHalfRes = viewSpec.GetRenderingHalfRes();

	const ShadingViewResourcesUsageInfo& resourcesUsageInfo = viewSpec.GetBlackboard().Get<ShadingViewResourcesUsageInfo>();

	std::swap(m_normalsTextureHalfRes, m_historyNormalsTextureHalfRes);

	if (!m_normalsTextureHalfRes || m_normalsTextureHalfRes->GetResolution2D() != renderingHalfRes)
	{
		rhi::TextureDefinition normalsDef;
		normalsDef.resolution = renderingHalfRes;
		normalsDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource);
		normalsDef.format     = rhi::EFragmentFormat::RG16_UN_Float;
		m_normalsTextureHalfRes = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Normals Texture Half Res"), normalsDef, rhi::EMemoryUsage::GPUOnly);
	}

	if (resourcesUsageInfo.useHalfResRoughnessWithHistory)
	{
		std::swap(m_roughnessTextureHalfRes, m_historyRoughnessTextureHalfRes);

		if (!m_roughnessTextureHalfRes || m_roughnessTextureHalfRes->GetResolution2D() != renderingHalfRes)
		{
			rhi::TextureDefinition roughnessDef;
			roughnessDef.resolution = renderingHalfRes;
			roughnessDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource);
			roughnessDef.format     = GBuffer::GetFormat(GBuffer::Texture::Roughness);
			m_roughnessTextureHalfRes = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Roughness Texture Half Res"), roughnessDef, rhi::EMemoryUsage::GPUOnly);
		}
	}
	else
	{
		m_roughnessTextureHalfRes.reset();
		m_historyRoughnessTextureHalfRes.reset();
	}

	if (resourcesUsageInfo.useHalfResBaseColorWithHistory)
	{
		std::swap(m_baseColorTextureHalfRes, m_historyBaseColorTextureHalfRes);

		if (!m_baseColorTextureHalfRes || m_baseColorTextureHalfRes->GetResolution2D() != renderingHalfRes)
		{
			rhi::TextureDefinition baseColorDef;
			baseColorDef.resolution = renderingHalfRes;
			baseColorDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource);
			baseColorDef.format     = rhi::EFragmentFormat::RGBA8_UN_Float;
			m_baseColorTextureHalfRes = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Base Color Texture Half Res"), baseColorDef, rhi::EMemoryUsage::GPUOnly);
		}
	}
	else
	{
		m_baseColorTextureHalfRes.reset();
		m_historyBaseColorTextureHalfRes.reset();
	}
}

} // spt::rsc
