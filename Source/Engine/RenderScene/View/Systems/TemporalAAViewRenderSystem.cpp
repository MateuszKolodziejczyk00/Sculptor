#include "TemporalAAViewRenderSystem.h"
#include "View/ViewRenderingSpec.h"
#include "Techniques/TemporalAA/TemporalAATypes.h"
#include "RenderGraphBuilder.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "SceneRenderer/RenderStages/Utils/RTReflectionsTypes.h"


namespace spt::rsc
{

RendererFloatParameter renderingResolutionScale("Rendering Resolution Scale", { "Temporal AA" }, 0.7f, 0.1f, 1.f);
RendererBoolParameter  enableUnifiedDenoising("Enable Unified Denoising", { "Temporal AA" }, false);
RendererBoolParameter  enableTransformerModel("Enable Transformer Model", { "Temporal AA" }, false);
RendererBoolParameter  debugDisableJitter("Debug Disable Jitter", { "Temporal AA" }, false);

namespace priv
{

BEGIN_SHADER_STRUCT(PrepareUnifiedDenoisingGuidingTexturesConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpResolution)
END_SHADER_STRUCT();


DS_BEGIN(PrepareUnifiedDenoisingGuidingTexturesDS, rg::RGDescriptorSetState<PrepareUnifiedDenoisingGuidingTexturesDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<PrepareUnifiedDenoisingGuidingTexturesConstants>), u_constants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                            u_depth)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                            u_roughness)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                                    u_baseColorMetallic)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                                    u_tangentFrame)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                                     u_rwDiffuseAlbedo)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                                     u_rwSpecularAlbedo)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                                     u_rwNormals)
DS_END();


static rdr::PipelineStateID CompilePrepareUnifiedDenoisingGuidingTexturesPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/UnifiedDenoising/PrepareUnifiedDenoisingGuidingTextures.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "PrepareUnifiedDenoisingGuidingTexturesCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("PrepareUnifiedDenoisingGuidingTexturesPipeline"), shader);
}


gfx::UnifiedDenoisingParams PrepareGuidingTextures(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	const RenderView& renderView = viewSpec.GetRenderView();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	rg::RGTextureViewHandle diffuseAlbedo  = graphBuilder.CreateTextureView(RG_DEBUG_NAME("UD Diffuse Albedo"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA8_UN_Float));
	rg::RGTextureViewHandle specularAlbedo = graphBuilder.CreateTextureView(RG_DEBUG_NAME("UD Specular Albedo"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA8_UN_Float));
	rg::RGTextureViewHandle normals        = graphBuilder.CreateTextureView(RG_DEBUG_NAME("UD Normals"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA16_S_Float));

	PrepareUnifiedDenoisingGuidingTexturesConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.rcpResolution = resolution.cast<Real32>().cwiseInverse();

	const lib::MTHandle<PrepareUnifiedDenoisingGuidingTexturesDS> prepareGuidingTexturesDS = graphBuilder.CreateDescriptorSet<PrepareUnifiedDenoisingGuidingTexturesDS>(RENDERER_RESOURCE_NAME("PrepareUnifiedDenoisingGuidingTexturesDS"));
	prepareGuidingTexturesDS->u_constants         = shaderConstants;
	prepareGuidingTexturesDS->u_depth             = viewContext.depth;
	prepareGuidingTexturesDS->u_roughness         = viewContext.gBuffer[GBuffer::Texture::Roughness];
	prepareGuidingTexturesDS->u_baseColorMetallic = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
	prepareGuidingTexturesDS->u_tangentFrame      = viewContext.gBuffer[GBuffer::Texture::TangentFrame];
	prepareGuidingTexturesDS->u_rwDiffuseAlbedo   = diffuseAlbedo;
	prepareGuidingTexturesDS->u_rwSpecularAlbedo  = specularAlbedo;
	prepareGuidingTexturesDS->u_rwNormals         = normals;

	static const rdr::PipelineStateID pipeline = CompilePrepareUnifiedDenoisingGuidingTexturesPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Prepare Unified Denoising Guiding Textures"),
						  pipeline,
						  math::Vector2u(math::Utils::DivideCeil(resolution.x(), 8u), math::Utils::DivideCeil(resolution.y(), 8u)),
						  rg::BindDescriptorSets(prepareGuidingTexturesDS, renderView.GetRenderViewDS()));

	const RTReflectionsViewData& reflectionsViewData = viewSpec.GetBlackboard().Get<RTReflectionsViewData>();

	rhi::TextureViewDefinition specularHitDistViewDef;
	specularHitDistViewDef.componentMappings.r = rhi::ETextureComponentMapping::A;

	const rg::RGTextureViewHandle specularHitDistanceView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Specular Hit Distance"),
																							reflectionsViewData.finalSpecularGI->GetTexture(),
																							specularHitDistViewDef);

	const SceneViewData& sceneViewData = renderView.GetViewRenderingData();

	const math::Matrix4f worldToView = sceneViewData.viewMatrix;
	const math::Matrix4f viewToClip  = sceneViewData.projectionMatrix;

	return gfx::UnifiedDenoisingParams
	{
		worldToView,
		viewToClip,
		diffuseAlbedo,
		specularAlbedo,
		normals,
		viewContext.gBuffer[GBuffer::Texture::Roughness],
		specularHitDistanceView
	};
}

} // priv


TemporalAAViewRenderSystem::TemporalAAViewRenderSystem()
{
}

void TemporalAAViewRenderSystem::SetTemporalAARenderer(lib::UniquePtr<gfx::TemporalAARenderer> temporalAARenderer)
{
	const lib::LockGuard lockGuard(m_temporalAARendererLock);

	m_requestedTemporalAARenderer = std::move(temporalAARenderer);
}

lib::StringView TemporalAAViewRenderSystem::GetRendererName() const
{
	const lib::LockGuard lockGuard(m_temporalAARendererLock);

	if (m_temporalAARenderer)
	{
		return m_temporalAARenderer->GetName();
	}

	return "None";
}

void TemporalAAViewRenderSystem::PrepareRenderView(RenderView& renderView)
{
	Super::PrepareRenderView(renderView);

	{
		const lib::LockGuard lockGuard(m_temporalAARendererLock);
		if (m_requestedTemporalAARenderer)
		{
			m_temporalAARenderer = std::move(*m_requestedTemporalAARenderer);
			m_requestedTemporalAARenderer.reset();
		}
	}

	Bool preparedAA = false;

	const math::Vector2u outputResolution           = renderView.GetOutputRes();
	const math::Vector2u desiredRenderingResolution = SelectDesiredRenderingResolution(outputResolution);

	if (m_temporalAARenderer)
	{
		gfx::TemporalAAParams aaParams;
		aaParams.inputResolution        = desiredRenderingResolution;
		aaParams.outputResolution       = renderView.GetOutputRes();
		aaParams.quality                = gfx::ETemporalAAQuality::Ultra;
		aaParams.flags                  = lib::Flags(gfx::ETemporalAAFlags::HDR, gfx::ETemporalAAFlags::DepthInverted, gfx::ETemporalAAFlags::MotionLowRes);
		aaParams.enableUnifiedDenoising = enableUnifiedDenoising;
		aaParams.enableTransformerModel = enableTransformerModel;

		preparedAA = m_temporalAARenderer->PrepareForRendering(aaParams);
	}

	if (preparedAA)
	{
		if (!debugDisableJitter)
		{
			const math::Vector2f jitter = m_temporalAARenderer->ComputeJitter(renderView.GetRenderedFrameIdx(), desiredRenderingResolution, outputResolution);
			renderView.SetJitter(jitter);
		}
		else
		{
			renderView.ResetJitter();
		}
		renderView.SetRenderingRes(desiredRenderingResolution);
	}
	else
	{
		renderView.ResetJitter();
		renderView.SetRenderingRes(renderView.GetOutputRes()); // fallback to output resolution
	}


	m_canRenderAA = preparedAA;
}

void TemporalAAViewRenderSystem::BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	Super::BeginFrame(graphBuilder, renderScene, viewSpec);

	if (m_canRenderAA)
	{
		SPT_CHECK(!!m_temporalAARenderer);
		SPT_CHECK(viewSpec.SupportsStage(ERenderStage::AntiAliasing))

		RenderStageEntries& stageEntries = viewSpec.GetRenderStageEntries(ERenderStage::AntiAliasing);
		stageEntries.GetOnRenderStage().AddRawMember(this, &TemporalAAViewRenderSystem::OnRenderAntiAliasingStage);

		if (m_temporalAARenderer->ExecutesUnifiedDenoising())
		{
			ShadingViewRenderingSystemsInfo& systemsInfo = viewSpec.GetBlackboard().Get<ShadingViewRenderingSystemsInfo>();
			systemsInfo.useUnifiedDenoising = true;
		}
	}
}

void TemporalAAViewRenderSystem::OnInitialize(RenderView& inRenderView)
{
	Super::OnInitialize(inRenderView);

	inRenderView.AddRenderStages(ERenderStage::AntiAliasing);
}

void TemporalAAViewRenderSystem::OnRenderAntiAliasingStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_temporalAARenderer);
	SPT_CHECK(m_canRenderAA);

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();
	const math::Vector2u outputRes    = viewSpec.GetRenderView().GetOutputRes();

	const RenderView& renderView = viewSpec.GetRenderView();
	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const rhi::EFragmentFormat texturesFormat = viewContext.luminance->GetFormat();

	const rg::RGTextureViewHandle outputColor = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Luminance Post AA"), rg::TextureDef(outputRes, texturesFormat));

	const rg::RGBufferViewHandle exposureBuffer = graphBuilder.AcquireExternalBufferView(renderView.GetExposureDataBuffer());

	const Uint32 exposureOffset	       = offsetof(ViewExposureData, exposure);
	const Uint32 historyExposureOffset = offsetof(ViewExposureData, exposureLastFrame);

	gfx::TemporalAARenderingParams renderingParams;
	renderingParams.depth                          = viewContext.depth;
	renderingParams.motion                         = viewContext.motion;
	renderingParams.inputColor                     = viewContext.luminance;
	renderingParams.outputColor                    = outputColor;
	renderingParams.jitter                         = renderView.GetJitter();
	renderingParams.sharpness                      = 0.0f;
	renderingParams.resetAccumulation              = context.rendererSettings.resetAccumulation;
	renderingParams.exposure.exposureBuffer        = exposureBuffer;
	renderingParams.exposure.exposureOffset        = exposureOffset;
	renderingParams.exposure.historyExposureOffset = historyExposureOffset;

	if (m_temporalAARenderer->ExecutesUnifiedDenoising())
	{
		renderingParams.unifiedDenoisingParams = priv::PrepareGuidingTextures(graphBuilder, viewSpec);
	}

	m_temporalAARenderer->Render(graphBuilder, renderingParams);

	viewContext.luminance = outputColor;
}

math::Vector2u TemporalAAViewRenderSystem::SelectDesiredRenderingResolution(math::Vector2u outputResolution) const
{
	const Bool supportsUpscaling = m_temporalAARenderer && m_temporalAARenderer->SupportsUpscaling();

	if (supportsUpscaling)
	{
		math::Vector2u resolution = (outputResolution.cast<Real32>() * renderingResolutionScale).cast<Uint32>();
		resolution.x() = std::min(math::Utils::DivideCeil(resolution.x(), 16u) * 16u, outputResolution.x());
		resolution.y() = std::min(math::Utils::DivideCeil(resolution.y(), 16u) * 16u, outputResolution.y());

		return resolution;
	}
	else
	{
		return outputResolution;
	}
}

} // spt::rsc
