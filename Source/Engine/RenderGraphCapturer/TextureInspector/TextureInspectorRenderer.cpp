#include "TextureInspectorRenderer.h"
#include "RenderGraphBuilder.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RGDescriptorSetState.h"
#include "ShaderStructs/ShaderStructs.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "Loaders/TextureLoader.h"


namespace spt::rg::capture
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

DS_BEGIN(TextureInspectorFilterDS, rg::RGDescriptorSetState<TextureInspectorFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4f>),                    u_floatTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4i>),                    u_intTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector4f>),                    u_floatTexture3D)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector4i>),                    u_intTexture3D)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TextureInspectorFilterParams>),            u_params)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                             u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<TextureInspectorReadbackData>),        u_readbackBuffer)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<math::Vector4u>),              u_histogram)
DS_END();


static rdr::PipelineStateID CompileTextureViewerFilterPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/TextureInspector/TextureInspectorFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TextureInspectorFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TextureInspectorFilterPipeline"), shader);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ViewedTextureRenderer =========================================================================

TextureInspectorRenderer::TextureInspectorRenderer()
{ }

void TextureInspectorRenderer::SetParameters(const TextureInspectorFilterParams& parameters)
{
	m_parameters = parameters;
}

void TextureInspectorRenderer::SaveTexture(SaveTextureParams param)
{
	SPT_CHECK(!m_saveTextureParams);
	m_saveTextureParams = param;
}

void TextureInspectorRenderer::Render(const lib::SharedRef<rdr::TextureView>& inputTexture, const lib::SharedRef<rdr::TextureView>& outputTexture, const lib::SharedRef<TextureInspectorReadback>& readback)
{
	SPT_PROFILER_FUNCTION();

	rg::RenderGraphBuilder graphBuilder(m_resourcesPool);

	const math::Vector2u resolution = outputTexture->GetResolution2D();

	const lib::SharedRef<rdr::Buffer> readbackBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Texture Inspector Readback"),
																						   rhi::BufferDefinition(sizeof(TextureInspectorReadbackData), rhi::EBufferUsage::Storage),
																						   rhi::EMemoryUsage::GPUToCpu);

	const rg::RGTextureViewHandle inputTextureView  = graphBuilder.AcquireExternalTextureView(inputTexture.ToSharedPtr());
	const rg::RGTextureViewHandle outputTextureView = graphBuilder.AcquireExternalTextureView(outputTexture.ToSharedPtr());

	const Bool isIntTexture = m_parameters.isIntTexture;
	
	lib::MTHandle<TextureInspectorFilterDS> filterDS = graphBuilder.CreateDescriptorSet<TextureInspectorFilterDS>(RENDERER_RESOURCE_NAME("TextureInspectorFilterDS"));
	filterDS->u_params = m_parameters;
	filterDS->u_outputTexture = outputTextureView;
	if (m_parameters.depthSlice3D != idxNone<Uint32>)
	{
		if (isIntTexture)
		{
			filterDS->u_intTexture3D = inputTextureView;
		}
		else
		{
			filterDS->u_floatTexture3D = inputTextureView;
		}
	}
	else
	{
		if (isIntTexture)
		{
			filterDS->u_intTexture = inputTextureView;
		}
		else
		{
			filterDS->u_floatTexture = inputTextureView;
		}
	}
	filterDS->u_readbackBuffer = graphBuilder.AcquireExternalBufferView(readbackBuffer->GetFullView());

	const Bool wantsHistogram = m_parameters.shouldOutputHistogram;
	lib::SharedPtr<rdr::Buffer> histogramReadbackBuffer;
	rg::RGBufferViewHandle rgHistogram;
	if (wantsHistogram)
	{
		rhi::BufferDefinition histogramDef;
		histogramDef.size  = sizeof(math::Vector4u) * TextureHistogram::binsNum;
		histogramDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferSrc, rhi::EBufferUsage::TransferDst);
		histogramReadbackBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Texture Histogram Readback"),
															  histogramDef,
															  rhi::EMemoryUsage::GPUToCpu);

		rgHistogram = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Texture Histogram"), histogramDef, rhi::EMemoryUsage::GPUOnly);

		filterDS->u_histogram = rgHistogram;

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Texture Histogram"), rgHistogram, 0u);
	}

	static const rdr::PipelineStateID pipelineState = CompileTextureViewerFilterPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Texture Viewer Filter Pass"),
						  pipelineState,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(filterDS)));

	if (wantsHistogram)
	{
		const rg::RGBufferViewHandle rgReadbackBuffer = graphBuilder.AcquireExternalBufferView(histogramReadbackBuffer->GetFullView());

		graphBuilder.CopyBuffer(RG_DEBUG_NAME("Copy Histogram Readback"), rgHistogram, 0u, rgReadbackBuffer, 0u, rgReadbackBuffer->GetSize());
	}

	if (m_saveTextureParams)
	{
		gfx::TextureWriter::SaveTexture(graphBuilder, outputTextureView, std::move(m_saveTextureParams->path));
		m_saveTextureParams.reset();
	}

	graphBuilder.ReleaseTextureWithTransition(outputTextureView->GetTexture(), rhi::TextureTransition::ShaderRead);

	// Schedule readback
	js::Launch(SPT_GENERIC_JOB_NAME,
			   [readbackData = readbackBuffer.ToSharedPtr(), weakReadback = readback.AsWeakPtr(), histogramReadbackBuffer]()
			   {
				   if (auto readback = weakReadback.lock())
				   {
					   TextureInspectorReadback::ReadbackPayload payload;
					   if(histogramReadbackBuffer)
					   {
						   const rhi::RHIMappedBuffer<math::Vector4u> mappedHistogram(histogramReadbackBuffer->GetRHI());
						   payload.histogram = TextureHistogram(mappedHistogram);
					   }

					   rhi::RHIMappedBuffer<TextureInspectorReadbackData> mappedReadback(readbackData->GetRHI());
					   readback->SetData(mappedReadback[0], payload);
				   }
			   },
			   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));

	graphBuilder.Execute();
}

} // spt::rg::capture
