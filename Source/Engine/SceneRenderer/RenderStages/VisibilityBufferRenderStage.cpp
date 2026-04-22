#include "VisibilityBufferRenderStage.h"
#include "Parameters/SceneRendererParams.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "SceneRenderSystems/StaticMeshes/StaticMeshesRenderSystem.h"
#include "SceneRenderSystems/Terrain/TerrainRenderSystem.h"
#include "Utils/hiZRenderer.h"
#include "Utils/Geometry/MaterialsRenderer.h"
#include "SceneRenderer/Utils/LinearizeDepth.h"
#include "SceneRenderer/Utils/GBufferUtils.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::VisibilityBuffer, VisibilityBufferRenderStage);

namespace renderer_params
{
RendererBoolParameter enablePOM("Enable POM", { "Geometry" }, true);
} // renderer_params

namespace utils
{

static lib::SharedPtr<rdr::TextureView> CreateDepthTexture(rdr::RendererResourceName name, rhi::EFragmentFormat format, math::Vector2u resolution, Bool isRenderTarget)
{
	rhi::TextureDefinition depthDef;
	depthDef.resolution	= resolution;
	depthDef.usage		= lib::Flags(rhi::ETextureUsage::SampledTexture, (isRenderTarget ? rhi::ETextureUsage::DepthSetncilRT : rhi::ETextureUsage::StorageTexture));
	depthDef.format		= format;

#if !SPT_RELEASE
	lib::AddFlag(depthDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE

	return rdr::ResourcesManager::CreateTextureView(name, depthDef, rhi::EMemoryUsage::GPUOnly);
}


BEGIN_SHADER_STRUCT(ApplyPOMOffsetConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>, depth)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>, pomDepth)
	SHADER_STRUCT_FIELD(math::Vector2u,               resolution)
END_SHADER_STRUCT();


DS_BEGIN(ApplyPOMOffsetDS, rg::RGDescriptorSetState<ApplyPOMOffsetDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ApplyPOMOffsetConstants>), u_constants)
DS_END()


GRAPHICS_PSO(ApplyPOMOffsetPSO)
{
	VERTEX_SHADER("Sculptor/GeometryRendering/ApplyPOMOffset.hlsl", ApplyPOMOffsetVS);
	FRAGMENT_SHADER("Sculptor/GeometryRendering/ApplyPOMOffset.hlsl", ApplyPOMOffsetFS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		rhi::GraphicsPipelineDefinition psoDef;
		psoDef.rasterizationDefinition.cullMode = rhi::ECullMode::None;
		psoDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D32_S_Float, rhi::ECompareOp::Always);
		pso = CompilePSO(compiler, psoDef, {});
	}
};


static void ApplyPOMOffset(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle depth, rg::RGTextureViewHandle pomDepth, rg::RGTextureViewHandle rwDepth)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = depth->GetResolution2D();

	ApplyPOMOffsetConstants constants;
	constants.depth      = depth;
	constants.pomDepth   = pomDepth;
	constants.resolution = resolution;

	const lib::MTHandle<ApplyPOMOffsetDS> ds = graphBuilder.CreateDescriptorSet<ApplyPOMOffsetDS>(RENDERER_RESOURCE_NAME("Apply POM Offset DS"));
	ds->u_constants = constants;

	rg::RGRenderTargetDef depthRT;
	depthRT.textureView    = rwDepth;
	depthRT.loadOperation  = rhi::ERTLoadOperation::DontCare;
	depthRT.storeOperation = rhi::ERTStoreOperation::Store;

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);
	renderPassDef.SetDepthRenderTarget(depthRT);

	graphBuilder.RenderPass(RG_DEBUG_NAME("Apply POM Offset"), 
							renderPassDef,
							rg::BindDescriptorSets(ds),
							[res = resolution](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), res.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), res));

								recorder.BindGraphicsPipeline(ApplyPOMOffsetPSO::pso);

								recorder.DrawInstances(3u, 1u);
							});
}

} // utils


rhi::EFragmentFormat VisibilityBufferRenderStage::GetDepthFormat()
{
	return rhi::EFragmentFormat::D32_S_Float;
}

VisibilityBufferRenderStage::VisibilityBufferRenderStage()
	: m_octahedronNormalsTexture(RENDERER_RESOURCE_NAME("Octahedron Normals Texture"))
{
	rhi::TextureDefinition octahedronNormalsDef;
	octahedronNormalsDef.format = rhi::EFragmentFormat::RG16_UN_Float;
	octahedronNormalsDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
	m_octahedronNormalsTexture.SetDefinition(octahedronNormalsDef);
}

void VisibilityBufferRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	PrepareDepthTextures(viewSpec);

	CreateGBuffer(graphBuilder, viewSpec);

	ExecuteVisbilityBufferRendering(graphBuilder, rendererInterface, renderScene, viewSpec, stageContext);

	RenderGBufferAdditionalTextures(graphBuilder, viewSpec);
}

void VisibilityBufferRenderStage::PrepareDepthTextures(const ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	std::swap(m_currentDepthTexture, m_historyDepthTexture);
	std::swap(m_currentHiZTexture, m_historyHiZTexture);
	std::swap(m_currentDepthHalfRes, m_historyDepthHalfRes);

	const math::Vector2u renderingRes     = viewSpec.GetRenderingRes();
	const math::Vector2u renderingHalfRes = math::Utils::DivideCeil(renderingRes, math::Vector2u(2, 2));

	if (!m_currentDepthTexture || m_currentDepthTexture->GetResolution2D() != renderingRes)
	{
		m_currentDepthTexture = utils::CreateDepthTexture(RENDERER_RESOURCE_NAME("Depth Texture"), GetDepthFormat(), renderingRes, true);
		m_currentDepthHalfRes = utils::CreateDepthTexture(RENDERER_RESOURCE_NAME("Depth Texture Half Res"), rhi::EFragmentFormat::D32_S_Float, renderingHalfRes, false);
	}

	const HiZ::HiZSizeInfo hiZSizeInfo = HiZ::ComputeHiZSizeInfo(renderingRes);
	if (!m_currentHiZTexture || m_currentHiZTexture->GetResolution2D() != hiZSizeInfo.resolution)
	{
		rhi::TextureDefinition hiZDef;
		hiZDef.resolution = hiZSizeInfo.resolution;
		hiZDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
#if !SPT_RELEASE
		lib::AddFlag(hiZDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE
		hiZDef.format     = rhi::EFragmentFormat::R32_S_Float;
		hiZDef.mipLevels  = hiZSizeInfo.mipLevels;
		m_currentHiZTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("HiZ Texture"), hiZDef, rhi::EMemoryUsage::GPUOnly);
	}
}

void VisibilityBufferRenderStage::ExecuteVisbilityBufferRendering(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Geometry Rendering");

	const math::Vector2u resolution = viewSpec.GetRenderingRes();
	
	ShadingViewContext& shadingContext = viewSpec.GetShadingViewContext();

	const StaticMeshesRenderSystem& staticMeshesSystem = rendererInterface.GetRenderSystemChecked<StaticMeshesRenderSystem>();
	const GeometryPassDataCollection& cachedGeometryPassData = staticMeshesSystem.GetCachedOpaqueGeometryPassData();
	
	const rg::RGTextureViewHandle depth        = graphBuilder.AcquireExternalTextureView(m_currentDepthTexture);
	const rg::RGTextureViewHandle hiZ          = graphBuilder.AcquireExternalTextureView(m_currentHiZTexture);
	const rg::RGTextureViewHandle historyDepth = m_historyDepthTexture ? graphBuilder.AcquireExternalTextureView(m_historyDepthTexture) : nullptr;
	const rg::RGTextureViewHandle historyHiZ   = m_historyHiZTexture ? graphBuilder.AcquireExternalTextureView(m_historyHiZTexture) : nullptr;

	const rg::RGTextureViewHandle visibilityTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Visibility Texture"), rg::TextureDef(viewSpec.GetRenderingRes(), rhi::EFragmentFormat::R32_U_Int));

	const Bool enablePOM = renderer_params::enablePOM;

	rg::RGTextureViewHandle rasterizedDepth = depth;
	if (enablePOM)
	{
		rasterizedDepth = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Rasterized Depth"), rg::TextureDef(resolution, rhi::EFragmentFormat::D32_S_Float));
	}

	VisPassParams visPassParams{ viewSpec, cachedGeometryPassData };
	visPassParams.geometryPassParams.historyHiZ = historyHiZ;
	visPassParams.geometryPassParams.depth      = rasterizedDepth;
	visPassParams.geometryPassParams.hiZ        = hiZ;
	visPassParams.visibilityTexture             = visibilityTexture;

	const VisPassResult geometryPassesResult = m_VisPassRenderer.RenderVisibility(graphBuilder, rendererInterface, visPassParams);

	if (const TerrainRenderSystem* terrainRenderSystem = rendererInterface.GetRenderSystem<TerrainRenderSystem>())
	{
		if (terrainRenderSystem->IsEnabled())
		{
			const TerrainVisibilityRenderParams terrainVisibilityParams
			{
				.viewSpec          = viewSpec,
				.depthTexture      = rasterizedDepth,
				.visibilityTexture = visibilityTexture
			};

			terrainRenderSystem->RenderVisibilityBuffer(graphBuilder, terrainVisibilityParams);
		}
	}

	rg::RGTextureViewHandle pomDepth;
	if (enablePOM)
	{
		pomDepth = graphBuilder.CreateTextureView(RG_DEBUG_NAME("POM Depth Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::R8_UN_Float));
	}

	{
		materials_renderer::MaterialsPassDefinition materialPassDef{ viewSpec };
		materialPassDef.depthTexture      = rasterizedDepth;
		materialPassDef.visibleMeshlets   = geometryPassesResult.visibleMeshlets;
		materialPassDef.visibilityTexture = visibilityTexture;
		materialPassDef.enablePOM         = enablePOM;
		materialPassDef.pomDepth          = pomDepth;

		materials_renderer::MaterialRenderCommands materialRenderCommands;
		materials_renderer::AppendGeometryMaterialsRenderCommands(graphBuilder, materialPassDef, cachedGeometryPassData, INOUT materialRenderCommands);

		if (const TerrainRenderSystem* terrainRenderSystem = rendererInterface.GetRenderSystem<TerrainRenderSystem>())
		{
			const MaterialBatchPermutation terrainMaterial
			{
				.SHADER       = terrainRenderSystem->GetTerrainMaterialShader(),
				.DOUBLE_SIDED = true
			};
			materials_renderer::AppendTerrainMaterialsRenderCommand(graphBuilder, materialPassDef, terrainMaterial, INOUT materialRenderCommands);
		}

		materials_renderer::RenderMaterials(graphBuilder, materialPassDef, materialRenderCommands);
	}

	if (enablePOM)
	{
		utils::ApplyPOMOffset(graphBuilder, rasterizedDepth, pomDepth, depth);
	}

	shadingContext.depth        = depth;
	shadingContext.depthHalfRes = graphBuilder.AcquireExternalTextureView(m_currentDepthHalfRes);
	shadingContext.historyDepth = historyDepth;
	if (m_historyDepthHalfRes)
	{
		shadingContext.historyDepthHalfRes = graphBuilder.AcquireExternalTextureView(m_historyDepthHalfRes);
	}

	shadingContext.hiZ = hiZ;

	DepthCullingParams depthCullingParams;
	depthCullingParams.hiZResolution = hiZ->GetResolution2D().cast<Real32>();

	lib::MTHandle<DepthCullingDS> depthCullingDS = graphBuilder.CreateDescriptorSet<DepthCullingDS>(RENDERER_RESOURCE_NAME("DepthCullingDS"));
	depthCullingDS->u_hiZTexture         = hiZ;
	depthCullingDS->u_depthCullingParams = depthCullingParams;
	shadingContext.depthCullingDS = std::move(depthCullingDS);
}

void VisibilityBufferRenderStage::CreateGBuffer(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	const ShadingViewResourcesUsageInfo& resourcesUsageInfo = viewSpec.GetBlackboard().Get<ShadingViewResourcesUsageInfo>();

	ShadingViewContext& shadingContext = viewSpec.GetShadingViewContext();

	GBuffer::GBufferExternalTextures externalTextures;
	if (resourcesUsageInfo.useBaseColorHistory)
	{
		std::swap(m_externalBaseColorTexture, m_externalHistoryBaseColorTexture);

		externalTextures[GBuffer::Texture::BaseColorMetallic] = &m_externalBaseColorTexture;

		if (m_externalHistoryBaseColorTexture)
		{
			if (m_externalHistoryBaseColorTexture->GetResolution2D() != resolution)
			{
				m_externalHistoryBaseColorTexture.reset();
			}
			else
			{
				shadingContext.historyBaseColor = graphBuilder.AcquireExternalTextureView(m_externalHistoryBaseColorTexture);
			}
		}
	}
	if (resourcesUsageInfo.useRoughnessHistory)
	{
		std::swap(m_externalRoughnessTexture, m_externalHistoryRoughnessTexture);

		externalTextures[GBuffer::Texture::Roughness] = &m_externalRoughnessTexture;

		if (m_externalHistoryRoughnessTexture)
		{
			if (m_externalHistoryRoughnessTexture->GetResolution2D() != resolution)
			{
				m_externalHistoryRoughnessTexture.reset();
			}
			else
			{
				shadingContext.historyRoughness = graphBuilder.AcquireExternalTextureView(m_externalHistoryRoughnessTexture);
			}
		}
	}

	shadingContext.gBuffer.Create(graphBuilder, resolution, externalTextures);

}

void VisibilityBufferRenderStage::RenderGBufferAdditionalTextures(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	const ShadingViewResourcesUsageInfo& resourcesUsageInfo = viewSpec.GetBlackboard().Get<ShadingViewResourcesUsageInfo>();

	ShadingViewContext& shadingContext = viewSpec.GetShadingViewContext();

	if (resourcesUsageInfo.useOctahedronNormalsWithHistory)
	{
		m_octahedronNormalsTexture.Update(resolution);
		shadingContext.octahedronNormals        = graphBuilder.AcquireExternalTextureView(m_octahedronNormalsTexture.GetCurrent());
		shadingContext.historyOctahedronNormals = graphBuilder.TryAcquireExternalTextureView(m_octahedronNormalsTexture.GetHistory());

		gbuffer_utils::oct_normals::GenerateOctahedronNormals(graphBuilder, shadingContext.gBuffer, shadingContext.octahedronNormals);
	}

	if (resourcesUsageInfo.useLinearDepth)
	{
		shadingContext.linearDepth = ExecuteLinearizeDepth(graphBuilder, viewSpec.GetRenderView(), shadingContext.depth);
	}
}

} // spt::rsc
