#include "ShadowMapRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "Shadows/ShadowsRenderingTypes.h"
#include "RenderGraphBuilder.h"
#include "Renderer.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "ResourcesManager.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/ConditionalBinding.h"
#include "Common/ShaderCompilationInput.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Utils/TextureMipsBuilder.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::ShadowMap, ShadowMapRenderStage);

namespace msm
{

BEGIN_SHADER_STRUCT(FilterMSMParams)
	SHADER_STRUCT_FIELD(Bool, linearizeDepth)
	SHADER_STRUCT_FIELD(Real32, nearPlane)
	SHADER_STRUCT_FIELD(Real32, farPlane)
END_SHADER_STRUCT();


DS_BEGIN(FilterMSMDS, rg::RGDescriptorSetState<FilterMSMDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_input)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_inputSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                             u_output)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<FilterMSMParams>),                         u_params)
DS_END();

static rdr::PipelineStateID CompileHorizontalMSMFilterPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("IS_HORIZONTAL", "1"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/FilterMSMShadows.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "FilterMSMShadowMapCS"), compilationSettings);
	
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("FilterMSMShadowsPipeline"), shader);
}

static rdr::PipelineStateID CompileVerticalMSMFilterPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("IS_HORIZONTAL", "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/FilterMSMShadows.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "FilterMSMShadowMapCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("FilterMSMShadowsPipeline"), shader);
}

} // msm

namespace vsm
{

DS_BEGIN(FilterVSMDS, rg::RGDescriptorSetState<FilterVSMDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConditionalBinding<"FILTER_DEPTH",
							gfx::OptionalSRVTexture2DBinding<Real32>>),         u_depth)
	DS_BINDING(BINDING_TYPE(gfx::ConditionalBinding<"!FILTER_DEPTH",
							gfx::OptionalSRVTexture2DBinding<math::Vector2f>>), u_moments)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),           u_output)
DS_END();


static rdr::PipelineStateID CompileVSMFilterPipeline(Bool depthFilter)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("FILTER_DEPTH", depthFilter ? "1" : "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/FilterVSMShadows.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "FilterVSMShadowMapCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("FilterVSMShadowsPipeline"), shader);
}

} // vsm

rhi::EFragmentFormat ShadowMapRenderStage::GetRenderedDepthFormat()
{
	return rhi::EFragmentFormat::D16_UN_Float;
}

ShadowMapRenderStage::ShadowMapRenderStage()
{ }

void ShadowMapRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const ShadowMapsManagerSubsystem& shadowMapsManager = renderScene.GetSceneSubsystemChecked<ShadowMapsManagerSubsystem>();
	const EShadowMappingTechnique defaultTechnique = shadowMapsManager.GetShadowMappingTechnique();

	const RenderSceneEntityHandle& viewEntity = viewSpec.GetRenderView().GetViewEntity();
	const ShadowMapViewComponent& shadowMapViewData = viewEntity.get<ShadowMapViewComponent>();
	
	const EShadowMappingTechnique technique = shadowMapViewData.techniqueOverride.value_or(defaultTechnique);

	const rg::RGTextureHandle shadowMapTexture = graphBuilder.AcquireExternalTexture(shadowMapViewData.shadowMap);
	
	switch (technique)
	{
	case EShadowMappingTechnique::DPCF:
		RenderDPCF(graphBuilder, renderScene, viewSpec, stageContext, shadowMapTexture);
		break;
	
	case EShadowMappingTechnique::VSM:
		RenderVSM(graphBuilder, renderScene, viewSpec, stageContext, shadowMapTexture);
		break;

	default:

		SPT_CHECK_NO_ENTRY();
	}
}

void ShadowMapRenderStage::RenderDepth(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, const rg::RGTextureViewHandle depthRenderTargetView)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= depthRenderTargetView;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	graphBuilder.RenderPass(RG_DEBUG_NAME("Shadow Map (Depth)"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[renderingRes](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingRes.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingRes));
							});

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

void ShadowMapRenderStage::RenderDPCF(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, const rg::RGTextureHandle shadowMap)
{
	rhi::TextureViewDefinition shadowMapViewDef;
	shadowMapViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Depth);
	const rg::RGTextureViewHandle shadowMapTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Shadow Map RT View"), shadowMap, shadowMapViewDef);

	RenderDepth(graphBuilder, renderScene, viewSpec, stageContext, shadowMapTextureView);
}

void ShadowMapRenderStage::RenderMSM(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, const rg::RGTextureHandle shadowMap)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	const math::Vector2u renderingRes = renderView.GetRenderingRes();

	const rg::RGTextureHandle depthRenderTarget = graphBuilder.CreateTexture(RG_DEBUG_NAME("MSM Depth RT"), rg::TextureDef(renderingRes, rhi::EFragmentFormat::D16_UN_Float));

	rhi::TextureViewDefinition shadowRTViewDef;
	shadowRTViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Depth);
	const rg::RGTextureViewHandle shadowMapTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("MSM Depth RT View"), depthRenderTarget, shadowRTViewDef);

	RenderDepth(graphBuilder, renderScene, viewSpec, stageContext, shadowMapTextureView);

	const rg::RGTextureViewHandle blurIntermediateTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("MSM Blur Intermediate Texture"), rg::TextureDef(renderingRes, rhi::EFragmentFormat::RGBA16_UN_Float));

	const std::optional<Real32> farPlane = renderView.GetFarPlane();

	msm::FilterMSMParams horizontalFilterParams;
	horizontalFilterParams.linearizeDepth = true;
	horizontalFilterParams.nearPlane = renderView.GetNearPlane();
	horizontalFilterParams.farPlane = farPlane.value();

	const lib::MTHandle<msm::FilterMSMDS> horizontalFilterMSMDS = graphBuilder.CreateDescriptorSet<msm::FilterMSMDS>(RENDERER_RESOURCE_NAME("FilterMSMDS (Horizontal)"));
	horizontalFilterMSMDS->u_input	= shadowMapTextureView;
	horizontalFilterMSMDS->u_output = blurIntermediateTexture;
	horizontalFilterMSMDS->u_params = horizontalFilterParams;

	static rdr::PipelineStateID horizontalBlurPipeline = msm::CompileHorizontalMSMFilterPipeline();

	const math::Vector3u horizontalBlurWorkCount = math::Utils::DivideCeil(renderView.GetRenderingRes3D(), math::Vector3u(256, 1, 1));
	graphBuilder.Dispatch(RG_DEBUG_NAME("MSM Horizontal Blur"),
						  horizontalBlurPipeline,
						  horizontalBlurWorkCount,
						  rg::BindDescriptorSets(horizontalFilterMSMDS));

	msm::FilterMSMParams verticalFilterParams;
	verticalFilterParams.linearizeDepth = false;

	rhi::TextureViewDefinition shadowMapMip0ViewDef;
	shadowMapMip0ViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, 0, 1);
	const rg::RGTextureViewHandle shadowMapMip0View = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Shadow Map View"), shadowMap, shadowMapMip0ViewDef);

	const lib::MTHandle<msm::FilterMSMDS> verticalFilterMSMDS = graphBuilder.CreateDescriptorSet<msm::FilterMSMDS>(RENDERER_RESOURCE_NAME("FilterMSMDS (Vertical)"));
	verticalFilterMSMDS->u_input	= blurIntermediateTexture;
	verticalFilterMSMDS->u_output	= shadowMapMip0View;
	verticalFilterMSMDS->u_params	= verticalFilterParams;

	static rdr::PipelineStateID verticalBlurPipeline = msm::CompileVerticalMSMFilterPipeline();

	const math::Vector3u verticalBlurWorkCount = math::Utils::DivideCeil(renderView.GetRenderingRes3D(), math::Vector3u(1, 256, 1));
	graphBuilder.Dispatch(RG_DEBUG_NAME("MSM Vertical Blur"),
						  verticalBlurPipeline,
						  verticalBlurWorkCount,
						  rg::BindDescriptorSets(verticalFilterMSMDS));

	MipsBuilder::BuildTextureMips(graphBuilder, shadowMap, 0, shadowMap->GetTextureDefinition().mipLevels);
}

void ShadowMapRenderStage::RenderVSM(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, const rg::RGTextureHandle shadowMap)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	const math::Vector2u renderingRes = renderView.GetRenderingRes();

	const rg::RGTextureViewHandle depthRenderTarget = graphBuilder.CreateTextureView(RG_DEBUG_NAME("VSM Depth RT"), rg::TextureDef(renderingRes, rhi::EFragmentFormat::D16_UN_Float));

	RenderDepth(graphBuilder, renderScene, viewSpec, stageContext, depthRenderTarget);

	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("VSM Temp Texture"), rg::TextureDef(renderingRes, shadowMap->GetFormat()));
	{
		// Horizontal pass

		const lib::MTHandle<vsm::FilterVSMDS> horizontalFilterDS = graphBuilder.CreateDescriptorSet<vsm::FilterVSMDS>(RENDERER_RESOURCE_NAME("FilterVSMDS (Horizontal)"));
		horizontalFilterDS->u_depth.Set(depthRenderTarget);
		horizontalFilterDS->u_output = tempTexture;

		static rdr::PipelineStateID horizontalBlurPipeline = vsm::CompileVSMFilterPipeline(true);

		graphBuilder.Dispatch(RG_DEBUG_NAME("VSM Horizontal Blur"),
							  horizontalBlurPipeline,
							  math::Utils::DivideCeil(renderingRes, math::Vector2u(128u, 1u)),
							  rg::BindDescriptorSets(horizontalFilterDS));
	}

	 
	{
		// Vertical pass

		rhi::TextureViewDefinition shadowMapMip0ViewDef;
		shadowMapMip0ViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, 0, 1);
		const rg::RGTextureViewHandle shadowMapMip0View = graphBuilder.CreateTextureView(RG_DEBUG_NAME("VSM Shadow Map View"), shadowMap, shadowMapMip0ViewDef);

		const lib::MTHandle<vsm::FilterVSMDS> verticalFilterDS = graphBuilder.CreateDescriptorSet<vsm::FilterVSMDS>(RENDERER_RESOURCE_NAME("FilterVSMDS (Vertical)"));
		verticalFilterDS->u_moments.Set(tempTexture);
		verticalFilterDS->u_output = shadowMapMip0View;

		static rdr::PipelineStateID verticalBlurPipeline = vsm::CompileVSMFilterPipeline(false);

		graphBuilder.Dispatch(RG_DEBUG_NAME("VSM Vertical Blur"),
							  verticalBlurPipeline,
							  math::Utils::DivideCeil(renderingRes, math::Vector2u(1u, 128u)),
							  rg::BindDescriptorSets(verticalFilterDS));
	}

	MipsBuilder::BuildTextureMips(graphBuilder, shadowMap, 0, shadowMap->GetTextureDefinition().mipLevels);
}

} // spt::rsc
