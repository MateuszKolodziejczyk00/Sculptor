#include "BRDFIntegrationLUT.h"
#include "RendererUtils.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "GPUApi.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"


namespace spt::rsc
{

namespace lut_generation
{

DS_BEGIN(BRDFIntegrationLUTGenerationDS, rg::RGDescriptorSetState<BRDFIntegrationLUTGenerationDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>), u_lut)
DS_END();


static rdr::PipelineStateID CreateLUTGenerationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/GenerateBRDFIntegrationLUT.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateBRDFIntegrationLUTCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Generate BRDF Integration LUT Pipeline"), shader);
}


static void GenerateLUT(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle lutHandle)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u lutResolution = lutHandle->GetResolution2D();

	lib::MTHandle<BRDFIntegrationLUTGenerationDS> ds = graphBuilder.CreateDescriptorSet<BRDFIntegrationLUTGenerationDS>(RENDERER_RESOURCE_NAME("BRDFIntegrationLUTGenerationDS"));
	ds->u_lut = lutHandle;
	
	const rdr::PipelineStateID lutGenerationPipeline = CreateLUTGenerationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Genrate BRDF Interation LUT"),
						  lutGenerationPipeline,
						  math::Utils::DivideCeil(lutResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // lut_generation

BRDFIntegrationLUT& BRDFIntegrationLUT::Get()
{
	static BRDFIntegrationLUT instance;
	return instance;
}

rg::RGTextureViewHandle BRDFIntegrationLUT::GetLUT(rg::RenderGraphBuilder& graphBuilder)
{
	if (!m_lut)
	{
		return CreateLUT(graphBuilder);
	}

	return graphBuilder.AcquireExternalTextureView(m_lut);
}

BRDFIntegrationLUT::BRDFIntegrationLUT()
{ }

rg::RGTextureViewHandle BRDFIntegrationLUT::CreateLUT(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!m_lut);

	const Uint32 lutSize = 512;

	rhi::TextureDefinition lutDef;
	lutDef.resolution = math::Vector2u(lutSize, lutSize);
	lutDef.format = rhi::EFragmentFormat::RG16_UN_Float;
	lutDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
	m_lut = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("BRDF Integration LUT"), lutDef, rhi::EMemoryUsage::GPUOnly);

	rdr::GPUApi::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_lut.reset();
															});

	const rg::RGTextureViewHandle lutHandle = graphBuilder.AcquireExternalTextureView(m_lut);

	lut_generation::GenerateLUT(graphBuilder, lutHandle);

	return lutHandle;
}

} // spt::rsc
