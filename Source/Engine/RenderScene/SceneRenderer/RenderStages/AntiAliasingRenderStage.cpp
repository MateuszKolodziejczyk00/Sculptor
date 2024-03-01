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

REGISTER_RENDER_STAGE(ERenderStage::AntiAliasing, AntiAliasingRenderStage);

namespace anti_aliasing
{

namespace temporal
{

BEGIN_SHADER_STRUCT(TemporalAAParams)
	SHADER_STRUCT_FIELD(Uint32, useYCoCg)
END_SHADER_STRUCT();


DS_BEGIN(TemporalAADS, rg::RGDescriptorSetState<TemporalAADS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depth)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_historyColor)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_inputColor)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_motion)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_outputColor)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),  u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TemporalAAParams>),                        u_params)
DS_END();


static rdr::PipelineStateID GetTemporalAAPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AntiAliasing/TemporalAA.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TemporalAACS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TemporalAAPipeline"), shader);
}


class TAATechnique : public AntiAliasingTechniqueInterface
{
protected:

	using Super = AntiAliasingTechniqueInterface;

public:

	TAATechnique()
		: Super(EAntiAliasingMode::TemporalAA)
	{ }

	// Begin AntiAliasingTechniqueInterface interface
	virtual void                    BeginFrame(const RenderView& renderView) override;
	virtual rg::RGTextureViewHandle Render(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle input) override;
	// End AntiAliasingTechniqueInterface interface

private:

	lib::SharedPtr<rdr::TextureView> m_historyTextureView;
	lib::SharedPtr<rdr::TextureView> m_currentTextureView;
};

void TAATechnique::BeginFrame(const RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	Super::BeginFrame(renderView);

	std::swap(m_historyTextureView, m_currentTextureView);

	const math::Vector2u renderingResolution = renderView.GetRenderingRes();

	if (!m_currentTextureView || m_currentTextureView->GetResolution2D() != renderingResolution)
	{
		rhi::TextureDefinition taaTextureDef;
		taaTextureDef.resolution = renderingResolution;
		taaTextureDef.usage      = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);
		taaTextureDef.format     = rhi::EFragmentFormat::RGBA16_S_Float;
		lib::SharedRef<rdr::Texture> outputTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("TAA Texture"), taaTextureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		m_currentTextureView = outputTexture->CreateView(RENDERER_RESOURCE_NAME("TAA Texture View"), viewDef);
	}
}

rg::RGTextureViewHandle TAATechnique::Render(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle input)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const rg::RGTextureViewHandle outputTexture = graphBuilder.AcquireExternalTextureView(m_currentTextureView);

	if (m_historyTextureView && m_historyTextureView->GetResolution2D() == input->GetResolution2D())
	{
		const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

		TemporalAAParams params;
		params.useYCoCg = false;

		const lib::MTHandle<TemporalAADS> temporalAADS = graphBuilder.CreateDescriptorSet<TemporalAADS>(RENDERER_RESOURCE_NAME("Temporal AA DS"));
		temporalAADS->u_depth        = viewContext.depth;
		temporalAADS->u_inputColor   = input;
		temporalAADS->u_historyColor = m_historyTextureView;
		temporalAADS->u_motion       = viewContext.motion;
		temporalAADS->u_outputColor  = outputTexture;
		temporalAADS->u_params       = params;

		static rdr::PipelineStateID temporalAAPipelineStateID = GetTemporalAAPipeline();

		graphBuilder.Dispatch(RG_DEBUG_NAME("Temporal AA"),
							  temporalAAPipelineStateID,
							  math::Utils::DivideCeil(viewSpec.GetRenderingRes(), math::Vector2u(8u, 8u)),
							  rg::BindDescriptorSets(temporalAADS));

		graphBuilder.CopyTexture(RG_DEBUG_NAME("Apply Temporal AA result"),
								 outputTexture, math::Vector3i::Zero(),
								 input, math::Vector3i::Zero(),
								 renderView.GetRenderingRes3D());

	}
	else
	{
		// We don't have history so we just copy the current frame
		graphBuilder.CopyTexture(RG_DEBUG_NAME("Save Temporal AA history"),
								 input, math::Vector3i::Zero(),
								 outputTexture, math::Vector3i::Zero(),
								 renderView.GetRenderingRes3D());
	}

	return outputTexture;
}

} // temporal

} // anti_aliasing

AntiAliasingRenderStage::AntiAliasingRenderStage()
{ }

void AntiAliasingRenderStage::BeginFrame(const RenderScene& renderScene, const RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	Super::BeginFrame(renderScene, renderView);

	PrepareAntiAliasingTechnique(renderView);

	if (m_technique)
	{
		m_technique->BeginFrame(renderView);
	}
}

void AntiAliasingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	if (m_technique)
	{
		viewContext.luminance = m_technique->Render(graphBuilder, viewSpec, viewContext.luminance);
	}
	
	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

void AntiAliasingRenderStage::PrepareAntiAliasingTechnique(const RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	const EAntiAliasingMode::Type aaMode = renderView.GetAntiAliasingMode();

	if (aaMode == EAntiAliasingMode::None)
	{
		m_technique.reset();
	}
	else if (!m_technique || m_technique->GetMode() != aaMode)
	{
		switch (aaMode)
		{
		case EAntiAliasingMode::TemporalAA:
			m_technique = std::make_unique<anti_aliasing::temporal::TAATechnique>();
			break;
		}
	}
}

} // spt::rsc
