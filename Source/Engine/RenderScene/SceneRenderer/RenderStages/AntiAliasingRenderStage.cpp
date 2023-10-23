#include "AntiAliasingRenderStage.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "Common/ShaderCompilationInput.h"

namespace spt::rsc
{

namespace anti_aliasing
{

namespace temporal
{

struct TemporalAAData
{
	lib::SharedPtr<rdr::TextureView> inputTextureView;
	lib::SharedPtr<rdr::TextureView> outputTextureView;
};


BEGIN_SHADER_STRUCT(TemporalAAParams)
	SHADER_STRUCT_FIELD(Bool, useYCoCg)
END_SHADER_STRUCT();


DS_BEGIN(TemporalAADS, rg::RGDescriptorSetState<TemporalAADS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depth)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_historyColor)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_inputColor)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_motion)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_outputColor)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<TemporalAAParams>),					u_params)
DS_END();


static TemporalAAData& GetTemporalAAData(const ViewRenderingSpec& viewSpec)
{
	const RenderView& renderView = viewSpec.GetRenderView();
	const RenderSceneEntityHandle viewEntity = renderView.GetViewEntity();
	
	TemporalAAData* temporalAAData = viewEntity.try_get<TemporalAAData>();
	if (!temporalAAData)
	{
		temporalAAData = &viewEntity.emplace<TemporalAAData>();
	}
	SPT_CHECK(!!temporalAAData);

	return *temporalAAData;
}

static rdr::PipelineStateID GetTemporalAAPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AntiAliasing/TemporalAA.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TemporalAACS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TemporalAAPipeline"), shader);
}

static rg::RGTextureViewHandle RenderTemporalAA(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	SPT_CHECK(renderView.IsJittering());

	const math::Vector2u renderingResolution = renderView.GetRenderingResolution();
	
	const ShadingData& shadingData = viewSpec.GetData().Get<ShadingData>();

	TemporalAAData& temporalAAData = GetTemporalAAData(viewSpec);

	// Reset history texture if it's deprecated
	if (temporalAAData.inputTextureView && temporalAAData.inputTextureView->GetResolution2D() != renderingResolution)
	{
		temporalAAData.inputTextureView.reset();
	}

	if (!temporalAAData.outputTextureView || temporalAAData.outputTextureView->GetResolution2D() != renderingResolution)
	{
		rhi::TextureDefinition taaTextureDef;
		taaTextureDef.resolution	= shadingData.luminanceTexture->GetResolution();
		taaTextureDef.usage			= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);
		taaTextureDef.format		= shadingData.luminanceTexture->GetFormat();
		lib::SharedRef<rdr::Texture> outputTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("TAA Texture"), taaTextureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		temporalAAData.outputTextureView = outputTexture->CreateView(RENDERER_RESOURCE_NAME("TAA Texture View"), viewDef);
	}

	const rg::RGTextureViewHandle inputTexture	= temporalAAData.inputTextureView ? graphBuilder.AcquireExternalTextureView(temporalAAData.inputTextureView) : nullptr;
	const rg::RGTextureViewHandle outputTexture	= temporalAAData.outputTextureView ? graphBuilder.AcquireExternalTextureView(temporalAAData.outputTextureView) : nullptr;

	if (inputTexture)
	{
		const DepthPrepassData& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassData>();
		const MotionData& motionData				= viewSpec.GetData().Get<MotionData>();

		TemporalAAParams params;
		params.useYCoCg = true;

		const lib::MTHandle<TemporalAADS> temporalAADS = graphBuilder.CreateDescriptorSet<TemporalAADS>(RENDERER_RESOURCE_NAME("Temporal AA DS"));
		temporalAADS->u_depth			= depthPrepassData.depth;
		temporalAADS->u_inputColor		= shadingData.luminanceTexture;
		temporalAADS->u_historyColor	= inputTexture;
		temporalAADS->u_motion			= motionData.motion;
		temporalAADS->u_outputColor		= outputTexture;
		temporalAADS->u_params			= params;

		static rdr::PipelineStateID temporalAAPipelineStateID = GetTemporalAAPipeline();

		const math::Vector3u dispatchGroups = math::Utils::DivideCeil(renderView.GetRenderingResolution3D(), math::Vector3u(8u, 8u, 1u));

		graphBuilder.Dispatch(RG_DEBUG_NAME("Temporal AA"),
							  temporalAAPipelineStateID,
							  dispatchGroups,
							  rg::BindDescriptorSets(temporalAADS));

		graphBuilder.CopyTexture(RG_DEBUG_NAME("Apply Temporal AA result"),
								 outputTexture, math::Vector3i::Zero(),
								 shadingData.luminanceTexture, math::Vector3i::Zero(),
								 renderView.GetRenderingResolution3D());
	}
	else
	{
		// We don't have history so we just copy the current frame
		graphBuilder.CopyTexture(RG_DEBUG_NAME("Save Temporal AA history"),
								 shadingData.luminanceTexture, math::Vector3i::Zero(),
								 outputTexture, math::Vector3i::Zero(),
								 renderView.GetRenderingResolution3D());
	}

	std::swap(temporalAAData.inputTextureView, temporalAAData.outputTextureView);

	return outputTexture;
}

} // temporal

} // anti_aliasing

AntiAliasingRenderStage::AntiAliasingRenderStage()
{ }

void AntiAliasingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	AntiAliasingData& aaData		= viewSpec.GetData().Create<AntiAliasingData>();
	const ShadingData& shadingData	= viewSpec.GetData().Get<ShadingData>();

	switch (renderView.GetAnitAliasingMode())
	{
	case EAntiAliasingMode::TemporalAA:
		aaData.outputTexture = anti_aliasing::temporal::RenderTemporalAA(graphBuilder, renderScene, viewSpec, stageContext);
		break;

	default:
		
		aaData.outputTexture = shadingData.luminanceTexture;
	}
	
	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
