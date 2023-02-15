#include "DepthPrepassRenderStage.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"

namespace spt::rsc
{

namespace HiZ
{

BEGIN_SHADER_STRUCT(BuildHiZParams)
	SHADER_STRUCT_FIELD(Uint32, downsampleMipsNum)
END_SHADER_STRUCT();


DS_BEGIN(BuildHiZDS, rg::RGDescriptorSetState<BuildHiZDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<BuildHiZParams>),					u_buildParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>),	u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_HiZMip0)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Real32>),								u_HiZMip1)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Real32>),								u_HiZMip2)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Real32>),								u_HiZMip3)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Real32>),								u_HiZMip4)
DS_END();


static rdr::PipelineStateID CompileBuildHiZPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildHiZCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DepthPrepass/BuildHiZ.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BuildHiZPipeline"), shader);
}

static rg::RGTextureViewHandle CreateHierarchicalZ(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle depthTexture, rhi::EFragmentFormat depthFormat)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector2u hiZRes = math::Vector2u(math::Utils::PreviousPowerOf2(renderingRes.x()), math::Utils::PreviousPowerOf2(renderingRes.y()));

	const Uint32 mipLevels = math::Utils::ComputeMipLevelsNumForResolution(hiZRes);

	rhi::TextureDefinition hiZTextureDef;
	hiZTextureDef.resolution	= math::Vector3u(hiZRes.x(), hiZRes.y(), 1u);
	hiZTextureDef.usage			= lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
	hiZTextureDef.format		= depthFormat;
	hiZTextureDef.mipLevels		= mipLevels;

	const rg::RGTextureHandle hiZTexture = graphBuilder.CreateTexture(RG_DEBUG_NAME("HiZ Texture"), hiZTextureDef, rhi::EMemoryUsage::GPUOnly);
	
	lib::DynamicArray<rg::RGTextureViewHandle> hiZMipViews;
	hiZMipViews.reserve(static_cast<SizeType>(mipLevels));

	for (Uint32 mipIdx = 0; mipIdx < mipLevels; ++mipIdx)
	{
		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, mipIdx, 1);
		hiZMipViews.emplace_back(graphBuilder.CreateTextureView(RG_DEBUG_NAME(std::format("HiZ Mip({})", mipIdx)), hiZTexture, viewDef));
	}

	static const rdr::PipelineStateID buildHiZPipeline = CompileBuildHiZPipeline();

	rg::RGTextureViewHandle inputDepthTexture = depthTexture;

	for (SizeType mipIdx = 0; mipIdx < static_cast<SizeType>(mipLevels); mipIdx += 5)
	{
		const Uint32 mipIdx0 = static_cast<Uint32>(mipIdx);
		const Uint32 downsampleMipsNum = std::min<Uint32>(mipLevels - mipIdx0, 5u);

		SPT_CHECK(downsampleMipsNum >= 1 && downsampleMipsNum <= 5);

		BuildHiZParams params;
		params.downsampleMipsNum = downsampleMipsNum;

		const lib::SharedRef<BuildHiZDS> buildHiZDS = rdr::ResourcesManager::CreateDescriptorSetState<BuildHiZDS>(RENDERER_RESOURCE_NAME(std::format("BuildHiZDS (Mips {} - {})", mipIdx, mipIdx0 + downsampleMipsNum)));
		buildHiZDS->u_buildParams = params;
		buildHiZDS->u_depthTexture = inputDepthTexture;
		buildHiZDS->u_HiZMip0 = hiZMipViews[mipIdx];
		if (downsampleMipsNum >= 2)
		{
			buildHiZDS->u_HiZMip1 = hiZMipViews[mipIdx + 1];
		}
		if (downsampleMipsNum >= 3)
		{
			buildHiZDS->u_HiZMip2 = hiZMipViews[mipIdx + 2];
		}
		if (downsampleMipsNum >= 4)
		{
			buildHiZDS->u_HiZMip3 = hiZMipViews[mipIdx + 3];
		}
		if (downsampleMipsNum >= 5)
		{
			buildHiZDS->u_HiZMip4 = hiZMipViews[mipIdx + 4];
		}

		const math::Vector2u outputRes(std::max(hiZRes.x() >> mipIdx, 1u), std::max(hiZRes.y() >> mipIdx, 1u));
		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 16u), math::Utils::DivideCeil(outputRes.y(), 16u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Build HiZ (Mips ({} - {})", mipIdx, mipIdx0 + downsampleMipsNum)),
							  buildHiZPipeline, 
							  groupCount,
							  rg::BindDescriptorSets(buildHiZDS));

		inputDepthTexture = hiZMipViews[mipIdx + downsampleMipsNum - 1];
	}

	rhi::TextureViewDefinition viewDefinition;
	viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("HiZ Texture"), hiZTexture, viewDefinition);
}

} // HiZ

rhi::EFragmentFormat DepthPrepassRenderStage::GetDepthFormat()
{
	return rhi::EFragmentFormat::D32_S_Float;
}

DepthPrepassRenderStage::DepthPrepassRenderStage()
{ }

void DepthPrepassRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u texturesRes(renderingRes.x(), renderingRes.y(), 1);

	const rhi::RHIAllocationInfo depthTextureAllocationInfo(rhi::EMemoryUsage::GPUOnly);

	DepthPrepassData& depthPrepassData = viewSpec.GetData().Create<DepthPrepassData>();

	const rhi::EFragmentFormat depthFormat = GetDepthFormat();

	rhi::TextureDefinition depthDef;
	depthDef.resolution	= texturesRes;
	depthDef.usage		= lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::DepthSetncilRT);
	depthDef.format		= depthFormat;
	depthPrepassData.depth = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DepthTexture"), depthDef, depthTextureAllocationInfo);

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= depthPrepassData.depth;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);
	
	const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingResolution();

	graphBuilder.RenderPass(RG_DEBUG_NAME("Depth Prepass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));
							});

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);

	depthPrepassData.hiZ = HiZ::CreateHierarchicalZ(graphBuilder, viewSpec, depthPrepassData.depth, rhi::EFragmentFormat::R32_S_Float);

	DepthCullingParams depthCullingParams;
	depthCullingParams.hiZResolution = depthPrepassData.hiZ->GetResolution2D().cast<Real32>();

	const lib::SharedRef<DepthCullingDS> depthCullingDS = rdr::ResourcesManager::CreateDescriptorSetState<DepthCullingDS>(RENDERER_RESOURCE_NAME("DepthCullingDS"));
	depthCullingDS->u_hiZTexture			= depthPrepassData.hiZ;
	depthCullingDS->u_depthCullingParams	= depthCullingParams;

	depthPrepassData.depthCullingDS = depthCullingDS;
}

} // spt::rsc
