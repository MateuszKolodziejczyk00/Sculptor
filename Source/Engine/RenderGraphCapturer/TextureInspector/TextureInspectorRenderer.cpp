#include "TextureInspectorRenderer.h"
#include "RenderGraphBuilder.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RGDescriptorSetState.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"


namespace spt::rg::capture
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

DS_BEGIN(TextureInspectorFilterDS, rg::RGDescriptorSetState<TextureInspectorFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4f>),             u_floatTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4i>),             u_intTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TextureInspectorFilterParams>),     u_params)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                      u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<TextureInspectorReadbackData>), u_readbackBuffer)
DS_END();


static rdr::PipelineStateID CompileTextureViewerFilterPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/TextureInspector/TextureInspectorFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TextureInspectorFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TextureInspectorilterPipeline"), shader);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ViewedTextureRenderer =========================================================================

TextureInspectorRenderer::TextureInspectorRenderer(const lib::SharedRef<rdr::TextureView>& inputTexture, const lib::SharedRef<rdr::TextureView>& outputTexture)
	: m_inputTexture(inputTexture)
	, m_outputTexture(outputTexture)
{ }

void TextureInspectorRenderer::SetParameters(const TextureInspectorFilterParams& parameters)
{
	m_parameters = parameters;
}

void TextureInspectorRenderer::Render(const lib::SharedRef<TextureInspectorReadback>& readback)
{
	SPT_PROFILER_FUNCTION();

	rg::RenderGraphBuilder graphBuilder(m_resourcesPool);

	const math::Vector2u resolution = m_outputTexture->GetResolution2D();

	const lib::SharedRef<rdr::Buffer> readbackBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Texture Inspector Readback"),
																						   rhi::BufferDefinition(sizeof(TextureInspectorReadbackData), rhi::EBufferUsage::Storage),
																						   rhi::EMemoryUsage::GPUToCpu);

	const rg::RGTextureViewHandle inputTextureView  = graphBuilder.AcquireExternalTextureView(m_inputTexture.ToSharedPtr());
	const rg::RGTextureViewHandle outputTextureView = graphBuilder.AcquireExternalTextureView(m_outputTexture.ToSharedPtr());


	const Bool isIntTexture = m_parameters.isIntTexture;
	
	lib::MTHandle<TextureInspectorFilterDS> filterDS = graphBuilder.CreateDescriptorSet<TextureInspectorFilterDS>(RENDERER_RESOURCE_NAME("TextureInspectorFilterDS"));
	filterDS->u_params = m_parameters;
	filterDS->u_outputTexture = outputTextureView;
	if (isIntTexture)
	{
		filterDS->u_intTexture = inputTextureView;
	}
	else
	{
		filterDS->u_floatTexture = inputTextureView;
	}
	filterDS->u_readbackBuffer = graphBuilder.AcquireExternalBufferView(readbackBuffer->CreateFullView());

	static const rdr::PipelineStateID pipelineState = CompileTextureViewerFilterPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Texture Viewer Filter Pass"),
						  pipelineState,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(filterDS)));

	graphBuilder.ReleaseTextureWithTransition(outputTextureView->GetTexture(), rhi::TextureTransition::FragmentReadOnly);

	// Schedule readback
	js::Launch(SPT_GENERIC_JOB_NAME,
			   [readbackData = readbackBuffer.ToSharedPtr(), weakReadback = readback.AsWeakPtr()]()
			   {
				   if (auto readback = weakReadback.lock())
				   {
					   rhi::RHIMappedBuffer<TextureInspectorReadbackData> mappedReadback(readbackData->GetRHI());
					   readback->SetData(mappedReadback[0]);
				   }
			   },
			   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));

	graphBuilder.Execute();
}

} // spt::rg::capture
