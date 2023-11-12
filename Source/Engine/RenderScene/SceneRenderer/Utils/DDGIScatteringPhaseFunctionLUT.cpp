#include "DDGIScatteringPhaseFunctionLUT.h"
#include "RendererUtils.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"


namespace spt::rsc
{

namespace lut_generation
{

DS_BEGIN(PreIntegrateDDGIScatteringPhaseFunctionDS, rg::RGDescriptorSetState<PreIntegrateDDGIScatteringPhaseFunctionDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>), u_lut)
DS_END();


static rdr::PipelineStateID CreateLUTGenerationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/PreIntegrateDDGIScatteringPhaseFunction.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "PreIntegrateDDGIScatteringPhaseFunctionCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Pre Integrate DDGI Scattering Phase Function"), shader);
}


static void GenerateLUT(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle lutHandle)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u lutResolution = lutHandle->GetResolution2D();

	lib::MTHandle<PreIntegrateDDGIScatteringPhaseFunctionDS> ds = graphBuilder.CreateDescriptorSet<PreIntegrateDDGIScatteringPhaseFunctionDS>(RENDERER_RESOURCE_NAME("PreIntegrateDDGIScatteringPhaseFunctionDS"));
	ds->u_lut = lutHandle;
	
	const rdr::PipelineStateID lutGenerationPipeline = CreateLUTGenerationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Pre Integrate DDGI Scattering Phase Function"),
						  lutGenerationPipeline,
						  math::Utils::DivideCeil(lutResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // lut_generation

DDGIScatteringPhaseFunctionLUT& DDGIScatteringPhaseFunctionLUT::Get()
{
	static DDGIScatteringPhaseFunctionLUT instance;
	return instance;
}

rg::RGTextureViewHandle DDGIScatteringPhaseFunctionLUT::GetLUT(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	if (!m_lut)
	{
		return CreateLUT(graphBuilder);
	}

	return graphBuilder.AcquireExternalTextureView(m_lut);
}

DDGIScatteringPhaseFunctionLUT::DDGIScatteringPhaseFunctionLUT()
{ }

rg::RGTextureViewHandle DDGIScatteringPhaseFunctionLUT::CreateLUT(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!m_lut);

	const Uint32 lutSize = 512;

	rhi::TextureDefinition lutDef;
	lutDef.resolution = math::Vector2u(lutSize, lutSize);
	lutDef.format = rhi::EFragmentFormat::R16_S_Float;
	lutDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
	m_lut = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("DDGI Scattering Phase Function LUT"), lutDef, rhi::EMemoryUsage::GPUOnly);

	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_lut.reset();
															});

	const rg::RGTextureViewHandle lutHandle = graphBuilder.AcquireExternalTextureView(m_lut);

	lut_generation::GenerateLUT(graphBuilder, lutHandle);

	return lutHandle;
}

} // spt::rsc
