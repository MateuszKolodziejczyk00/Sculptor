#include "TextureInspectorRenderer.h"
#include "Bindless/BindlessTypes.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Loaders/TextureLoader.h"


namespace spt::rg::capture
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

BEGIN_SHADER_STRUCT(TextureInspectorFilterConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,                   floatTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4i>,                   intTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>,                   floatTexture3D)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4i>,                   intTexture3D)
	SHADER_STRUCT_FIELD(TextureInspectorFilterParams,                        params)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,                   outputTexture)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<TextureInspectorReadbackData>, readbackBuffer)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<math::Vector4u>,                    histogram)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileTextureViewerFilterPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/TextureInspector/TextureInspectorFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TextureInspectorFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TextureInspectorFilterPipeline"), shader);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ViewedTextureRenderer =========================================================================

TextureInspectorRenderer::TextureInspectorRenderer()
	: m_memoryArena("Texture Inspector Renderer Arena", 1024u * 1024u, 8u * 1024u * 1024u)
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

	m_memoryArena.Reset();

	rg::RenderGraphBuilder graphBuilder(m_memoryArena, m_resourcesPool);

	const math::Vector2u resolution = outputTexture->GetResolution2D();

	const lib::SharedRef<rdr::Buffer> readbackBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Texture Inspector Readback"),
																						   rhi::BufferDefinition(sizeof(TextureInspectorReadbackData), rhi::EBufferUsage::Storage),
																						   rhi::EMemoryUsage::GPUToCpu);

	const rg::RGTextureViewHandle inputTextureView  = graphBuilder.AcquireExternalTextureView(inputTexture.ToSharedPtr());
	const rg::RGTextureViewHandle outputTextureView = graphBuilder.AcquireExternalTextureView(outputTexture.ToSharedPtr());

	const Bool isIntTexture = m_parameters.isIntTexture;
	
	TextureInspectorFilterConstants shaderConstants;
	shaderConstants.params = m_parameters;
	shaderConstants.outputTexture = outputTextureView;
	if (m_parameters.depthSlice3D != idxNone<Uint32>)
	{
		if (isIntTexture)
		{
			shaderConstants.intTexture3D = inputTextureView;
		}
		else
		{
			shaderConstants.floatTexture3D = inputTextureView;
		}
	}
	else
	{
		if (isIntTexture)
		{
			shaderConstants.intTexture = inputTextureView;
		}
		else
		{
			shaderConstants.floatTexture = inputTextureView;
		}
	}
	shaderConstants.readbackBuffer = graphBuilder.AcquireExternalBufferView(readbackBuffer->GetFullView());

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

		shaderConstants.histogram = rgHistogram;

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Texture Histogram"), rgHistogram, 0u);
	}

	static const rdr::PipelineStateID pipelineState = CompileTextureViewerFilterPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Texture Viewer Filter Pass"),
						  pipelineState,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));

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
