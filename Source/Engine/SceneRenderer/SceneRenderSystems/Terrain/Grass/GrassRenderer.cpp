#include "GrassRenderer.h"
#include "Utils/IndirectUtils.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include <Utils/ViewRenderingSpec.h>


namespace spt::rsc::grass_renderer
{

SIMPLE_COMPUTE_PSO(GenerateGrassBladesBufferPSO, "Sculptor/Terrain/Grass/GenerateGrassBladesDefs.hlsl", GenerateGrassBladesDefsCS);


using RWBladeDefsLODs = lib::StaticArray<gfx::RWTypedBuffer<GrassBladeDef>, 2u>;
using RWBladesNumLODs = lib::StaticArray<gfx::RWTypedBuffer<Uint32>, 2u>;

BEGIN_SHADER_STRUCT(GenerateGrassBladesDefsConstants)
	SHADER_STRUCT_FIELD(math::Vector2i,                    originTile)
	SHADER_STRUCT_FIELD(RWBladeDefsLODs,                   rwBladeDefsLODs)
	SHADER_STRUCT_FIELD(RWBladesNumLODs,                   rwBladesNumLODs)
END_SHADER_STRUCT();


GrassBlades GenerateGrassBlades(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, const GrassFieldDefinition& def)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 maxBladesNum = 1024u * def.tilesResolution.x() * def.tilesResolution.y() / 4u;
	
	const auto allocateGrassLODData = [&graphBuilder](Uint32 maxBladesNum) -> GrassBlades::LOD
	{
		const rg::RGBufferViewHandle bladesBuffer    = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Grass Blades Buffer"), sizeof(rdr::HLSLStorage<GrassBladeDef>) * maxBladesNum);
		const rg::RGBufferViewHandle bladesNumBuffer = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Grass Blades Num Buffer"), sizeof(rdr::HLSLStorage<Uint32>));

		return GrassBlades::LOD
		{
			.bladeDefs = bladesBuffer,
			.bladesNum = bladesNumBuffer
		};
	};

	GrassBlades blades;
	blades.lods[0] = allocateGrassLODData(maxBladesNum);
	blades.lods[1] = allocateGrassLODData(maxBladesNum);

	for (const GrassBlades::LOD& lod : blades.lods)
	{
		graphBuilder.MemZeroBuffer(lod.bladesNum);
	}

	RWBladeDefsLODs rwBladeDefsLODs;
	RWBladesNumLODs rwBladesNumLODs;
	for (Uint32 lodIdx = 0; lodIdx < 2u; ++lodIdx)
	{
		rwBladeDefsLODs[lodIdx] = blades.lods[lodIdx].bladeDefs;
		rwBladesNumLODs[lodIdx] = blades.lods[lodIdx].bladesNum;
	}

	GenerateGrassBladesDefsConstants shaderConstants;
	shaderConstants.originTile      = def.originTile;
	shaderConstants.rwBladeDefsLODs = rwBladeDefsLODs;
	shaderConstants.rwBladesNumLODs = rwBladesNumLODs;


	const math::Vector2u dispatchGroups = def.tilesResolution.cast<Uint32>();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Generate Grass Blades Buffer"),
						  GenerateGrassBladesBufferPSO::pso,
						  dispatchGroups,
						  rg::EmptyDescriptorSets(),
						  shaderConstants);

	return blades;
}


using BladeDefsLODs = lib::StaticArray<gfx::TypedBuffer<GrassBladeDef>, 2u>;
using BladesNumLODs = lib::StaticArray<gfx::TypedBuffer<Uint32>, 2u>;


BEGIN_SHADER_STRUCT(GrassBladesVisibilityPassConstants)
	SHADER_STRUCT_FIELD(BladeDefsLODs, bladeDefsLODs)
	SHADER_STRUCT_FIELD(BladesNumLODs, bladesNumLODs)
END_SHADER_STRUCT();


DS_BEGIN(GrassBladesVisibilityPassDS, rg::RGDescriptorSetState<GrassBladesVisibilityPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GrassBladesVisibilityPassConstants>), u_constants)
DS_END()


BEGIN_SHADER_STRUCT(GrassBladesVisibilityPassPSOPermutation)
	SHADER_STRUCT_FIELD(Int32, GRASS_BLADES_LOD)
END_SHADER_STRUCT();


GRAPHICS_PSO(GrassBladesVisibilityPassPSO)
{
	MESH_SHADER("Sculptor/Terrain/Grass/GrassBladesVisibilityPass.hlsl", GrassBladesVisibilityPassMS);
	FRAGMENT_SHADER("Sculptor/Terrain/Grass/GrassBladesVisibilityPass.hlsl", GrassBladesVisibilityPassFS);

	PRESET(lods)[2];

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		rhi::GraphicsPipelineDefinition psoDef;
		psoDef.rasterizationDefinition.cullMode = rhi::ECullMode::None;
		psoDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(
			rhi::ColorRenderTargetDefinition
			{
				.format         = rhi::EFragmentFormat::R32_U_Int,
				.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
				.alphaBlendType = rhi::ERenderTargetBlendType::Disabled,
			});

		psoDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D32_S_Float, rhi::ECompareOp::Greater);
		for (Int32 lodIdx = 0; lodIdx < 2; ++lodIdx)
		{
			lods[lodIdx] = CompilePermutation(compiler, psoDef, GrassBladesVisibilityPassPSOPermutation{ .GRASS_BLADES_LOD = lodIdx });
		}
	}
};


BEGIN_RG_NODE_PARAMETERS_STRUCT(GrassVisibilityParameters)
	RG_BUFFER_VIEW(lod0DrawCall, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(lod1DrawCall, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


void RenderGrassVisibility(rg::RenderGraphBuilder& graphBuilder, const GrassVisibilityRenderParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.depthTexture.IsValid());
	SPT_CHECK(params.visibilityTexture.IsValid());
	SPT_CHECK(params.depthTexture->GetResolution2D() == params.visibilityTexture->GetResolution2D());

	const math::Vector2u resolution = params.viewSpec.GetRenderingRes();

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = params.depthTexture;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	rg::RGRenderTargetDef visibilityRTDef;
	visibilityRTDef.textureView    = params.visibilityTexture;
	visibilityRTDef.loadOperation  = rhi::ERTLoadOperation::Load;
	visibilityRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	renderPassDef.AddColorRenderTarget(visibilityRTDef);

	GrassVisibilityParameters indirectParams;
	indirectParams.lod0DrawCall = gfx::indirect_utils::CreateIndirectDispatchMeshCommand(graphBuilder, params.grassBlades.lods[0].bladesNum, 9u);
	indirectParams.lod1DrawCall = gfx::indirect_utils::CreateIndirectDispatchMeshCommand(graphBuilder, params.grassBlades.lods[1].bladesNum, 9u);

	BladeDefsLODs bladeDefsLODs;
	BladesNumLODs bladesNumLODs;

	for (Uint32 lodIdx = 0; lodIdx < 2u; ++lodIdx)
	{
		bladeDefsLODs[lodIdx] = params.grassBlades.lods[lodIdx].bladeDefs;
		bladesNumLODs[lodIdx] = params.grassBlades.lods[lodIdx].bladesNum;
	}

	GrassBladesVisibilityPassConstants shaderConstants;
	shaderConstants.bladeDefsLODs = bladeDefsLODs;
	shaderConstants.bladesNumLODs = bladesNumLODs;

	lib::MTHandle<GrassBladesVisibilityPassDS> ds = graphBuilder.CreateDescriptorSet<GrassBladesVisibilityPassDS>(RENDERER_RESOURCE_NAME("Grass Visibility DS"));
	ds->u_constants = shaderConstants;

	graphBuilder.RenderPass(RG_DEBUG_NAME("Grass Blades Visibility Pass"),
							renderPassDef,
							rg::BindDescriptorSets(ds),
							std::tie(indirectParams),
							[resolution, indirectParams](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								{
									recorder.BindGraphicsPipeline(GrassBladesVisibilityPassPSO::lods[0]);
									const rdr::BufferView& drawCallView = *indirectParams.lod0DrawCall->GetResource();
									recorder.DrawMeshTasksIndirect(drawCallView.GetBuffer(), drawCallView.GetOffset(), sizeof(rdr::HLSLStorage<gfx::IndirectDispatchCommand>), 1u);
								}

								{
									recorder.BindGraphicsPipeline(GrassBladesVisibilityPassPSO::lods[1]);
									const rdr::BufferView& drawCallView = *indirectParams.lod1DrawCall->GetResource();
									recorder.DrawMeshTasksIndirect(drawCallView.GetBuffer(), drawCallView.GetOffset(), sizeof(rdr::HLSLStorage<gfx::IndirectDispatchCommand>), 1u);
								}
							});
}

} // spt::rsc::grass_renderer
