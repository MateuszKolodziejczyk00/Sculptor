#include "ShadowMapRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "Shadows/ShadowsRenderingTypes.h"
#include "RenderGraphBuilder.h"
#include "Renderer.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "Shadows/ShadowMapsManagerSystem.h"
#include "ResourcesManager.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Common/ShaderCompilationInput.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Utils/TextureMipsBuilder.h"

namespace spt::rsc
{

namespace msm
{

BEGIN_SHADER_STRUCT(FilterMSMParams)
	SHADER_STRUCT_FIELD(Bool, linearizeDepth)
	SHADER_STRUCT_FIELD(Real32, nearPlane)
	SHADER_STRUCT_FIELD(Real32, farPlane)
END_SHADER_STRUCT();


DS_BEGIN(FilterMSMDS, rg::RGDescriptorSetState<FilterMSMDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_input)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_inputSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_output)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<FilterMSMParams>),					u_params)
DS_END();

static rdr::PipelineStateID CompileHorizontalMSMFilterPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "FilterMSMShadowMapCS"));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("IS_HORIZONTAL", "1"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/FilterMSMShadows.hlsl", compilationSettings);
	
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("FilterMSMShadowsPipeline"), shader);
}

static rdr::PipelineStateID CompileVerticalMSMFilterPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "FilterMSMShadowMapCS"));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("IS_HORIZONTAL", "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/FilterMSMShadows.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("FilterMSMShadowsPipeline"), shader);
}

} // msm

rhi::EFragmentFormat ShadowMapRenderStage::GetRenderedDepthFormat()
{
	return rhi::EFragmentFormat::D16_UN_Float;
}

ShadowMapRenderStage::ShadowMapRenderStage()
{ }

void ShadowMapRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const ShadowMapsManagerSystem& shadowMapsManager = renderScene.GetPrimitivesSystemChecked<ShadowMapsManagerSystem>();
	const EShadowMappingTechnique technique = shadowMapsManager.GetShadowMappingTechnique();

	const RenderSceneEntityHandle& viewEntity = viewSpec.GetRenderView().GetViewEntity();
	const ShadowMapViewComponent& shadowMapViewData = viewEntity.get<ShadowMapViewComponent>();

	const rg::RGTextureHandle shadowMapTexture = graphBuilder.AcquireExternalTexture(shadowMapViewData.shadowMap);
	
	switch (technique)
	{
	case EShadowMappingTechnique::DPCF:
		RenderDPCF(graphBuilder, renderScene, viewSpec, stageContext, shadowMapTexture);
		break;
	
	case EShadowMappingTechnique::MSM:
		RenderMSM(graphBuilder, renderScene, viewSpec, stageContext, shadowMapTexture);
		break;

	default:

		SPT_CHECK_NO_ENTRY();
	}
}

void ShadowMapRenderStage::RenderDepth(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, const rg::RGTextureViewHandle depthRenderTargetView)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();

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

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
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

	rhi::TextureDefinition shadowRTDef;
	shadowRTDef.resolution = renderView.GetRenderingResolution3D();
	shadowRTDef.usage = lib::Flags(rhi::ETextureUsage::DepthSetncilRT, rhi::ETextureUsage::SampledTexture);
	shadowRTDef.format = rhi::EFragmentFormat::D16_UN_Float;
	const rg::RGTextureHandle depthRenderTarget = graphBuilder.CreateTexture(RG_DEBUG_NAME("MSM Depth RT"), shadowRTDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition shadowRTViewDef;
	shadowRTViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Depth);
	const rg::RGTextureViewHandle shadowMapTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("MSM Depth RT View"), depthRenderTarget, shadowRTViewDef);

	RenderDepth(graphBuilder, renderScene, viewSpec, stageContext, shadowMapTextureView);

	rhi::TextureDefinition blurIntermediateTextureDef;
	blurIntermediateTextureDef.resolution = renderView.GetRenderingResolution3D();
	blurIntermediateTextureDef.usage = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
	blurIntermediateTextureDef.format = rhi::EFragmentFormat::RGBA16_UN_Float;
	const rg::RGTextureViewHandle blurIntermediateTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("MSM Blur Intermediate Texture"), blurIntermediateTextureDef, rhi::EMemoryUsage::GPUOnly);

	const std::optional<Real32> farPlane = renderView.GetFarPlane();

	msm::FilterMSMParams horizontalFilterParams;
	horizontalFilterParams.linearizeDepth = true;
	horizontalFilterParams.nearPlane = renderView.GetNearPlane();
	horizontalFilterParams.farPlane = farPlane.value();

	const lib::SharedRef<msm::FilterMSMDS> horizontalFilterMSMDS = rdr::ResourcesManager::CreateDescriptorSetState<msm::FilterMSMDS>(RENDERER_RESOURCE_NAME("FilterMSMDS (Horizontal)"));
	horizontalFilterMSMDS->u_input	= shadowMapTextureView;
	horizontalFilterMSMDS->u_output = blurIntermediateTexture;
	horizontalFilterMSMDS->u_params = horizontalFilterParams;

	static rdr::PipelineStateID horizontalBlurPipeline = msm::CompileHorizontalMSMFilterPipeline();

	const math::Vector3u horizontalBlurWorkCount = math::Utils::DivideCeil(renderView.GetRenderingResolution3D(), math::Vector3u(256, 1, 1));
	graphBuilder.Dispatch(RG_DEBUG_NAME("MSM Horizontal Blur"),
						  horizontalBlurPipeline,
						  horizontalBlurWorkCount,
						  rg::BindDescriptorSets(horizontalFilterMSMDS));

	msm::FilterMSMParams verticalFilterParams;
	verticalFilterParams.linearizeDepth = false;

	rhi::TextureViewDefinition shadowMapMip0ViewDef;
	shadowMapMip0ViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, 0, 1);
	const rg::RGTextureViewHandle shadowMapMip0View = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Shadow Map View"), shadowMap, shadowMapMip0ViewDef);

	const lib::SharedRef<msm::FilterMSMDS> verticalFilterMSMDS = rdr::ResourcesManager::CreateDescriptorSetState<msm::FilterMSMDS>(RENDERER_RESOURCE_NAME("FilterMSMDS (Vertical)"));
	verticalFilterMSMDS->u_input	= blurIntermediateTexture;
	verticalFilterMSMDS->u_output	= shadowMapMip0View;
	verticalFilterMSMDS->u_params	= verticalFilterParams;

	static rdr::PipelineStateID verticalBlurPipeline = msm::CompileVerticalMSMFilterPipeline();

	const math::Vector3u verticalBlurWorkCount = math::Utils::DivideCeil(renderView.GetRenderingResolution3D(), math::Vector3u(1, 256, 1));
	graphBuilder.Dispatch(RG_DEBUG_NAME("MSM Vertical Blur"),
						  verticalBlurPipeline,
						  verticalBlurWorkCount,
						  rg::BindDescriptorSets(verticalFilterMSMDS));

	MipsBuilder::BuildTextureMips(graphBuilder, shadowMap, 0, shadowMap->GetTextureDefinition().mipLevels);
}

} // spt::rsc
