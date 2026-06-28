#include "PlacementSystemBackend.h"
#include "EngineFrame.h"
#include "ResourcesManager.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(GPUPlacementEntry)
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Real32,         scale)
	SHADER_STRUCT_FIELD(Uint32,         seed)
	SHADER_STRUCT_FIELD(Uint32,         prefabIdx)
	SHADER_STRUCT_FIELD(Uint32,         entryIdx)
END_SHADER_STRUCT();


struct PlacementExe
{
	lib::SharedPtr<rdr::Buffer> entries;
	lib::SharedPtr<rdr::Buffer> entriesNum;

	std::atomic<Bool> isFinished;
	std::atomic<Bool> isProcessed;

	Uint32 placementDefIdx = 0u;
};


namespace process_impl
{

static void ProcessPlacementResults(PlacementProcessor processor, void* customData, PlacementExe& exe)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(exe.isFinished.load());
	SPT_CHECK(!exe.isProcessed.load());
	SPT_CHECK(!!exe.entries);
	SPT_CHECK(!!exe.entriesNum);

	const rhi::RHIMappedBuffer<Uint32> mappedEntriesNum(exe.entriesNum->GetRHI());
	const rhi::RHIMappedBuffer<GPUPlacementEntry> mappedEntries(exe.entries->GetRHI());

	const Uint32 entriesNum = mappedEntriesNum[0];

	if (entriesNum > 0u)
	{
		const lib::Span<const GPUPlacementEntry> entries(mappedEntries.Get(), entriesNum);
		const lib::Span<const PlacementEntry> convertedEntries(reinterpret_cast<const PlacementEntry*>(entries.data()), entriesNum);

		PlacementProcessData processData;
		processData.placements      = convertedEntries;
		processData.placementDefIdx = exe.placementDefIdx;

		processor(customData, processData);
	}

	exe.isProcessed.store(true);
}


static void ProcessPlacementsResults(PlacementProcessor processor, void* customData, lib::Span<PlacementExe*> executions)
{
	for (PlacementExe* exe : executions)
	{
		if (exe->isFinished.load() && !exe->isProcessed.load())
		{
			ProcessPlacementResults(processor, customData, *exe);
		}
	}
}

} // process_impl

namespace render_impl

{
BEGIN_SHADER_STRUCT(PlacementConstants)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<GPUPlacementEntry>, rwEntries)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,            rwEntriesNum)
	SHADER_STRUCT_FIELD(Real32,                                placementSpacing)
	SHADER_STRUCT_FIELD(Uint32,                                resolution)
	SHADER_STRUCT_FIELD(PlacementPrefabsCollection,            prefabsCollection)
	SHADER_STRUCT_FIELD(math::Vector2i,                        beginCoords)
	SHADER_STRUCT_FIELD(math::Vector2i,                        endCoords)
	SHADER_STRUCT_FIELD(math::Vector2i,                        lastBeginCoords)
	SHADER_STRUCT_FIELD(math::Vector2i,                        lastEndCoords)
	SHADER_STRUCT_FIELD(Bool,                                  lastCoordsValid)
END_SHADER_STRUCT();


SIMPLE_COMPUTE_PSO(PlacementShaderPSO, "Sculptor/PlacementSystem/PlacementShader.hlsl", ComputePlacementsCS);


void InitializePlacementExe(PlacementExe& exe)
{
	const Uint64 entriesBufferSize = 8u * 1024u * sizeof(rdr::HLSLStorage<GPUPlacementEntry>);
	const Uint64 entriesNumBufferSize = sizeof(Uint32);
	exe.entries    = rdr::ResourcesManager::CreateStorageBuffer(RENDERER_RESOURCE_NAME("Placements Entries Buffer"), entriesBufferSize, rhi::EMemoryUsage::GPUToCpu);
	exe.entriesNum = rdr::ResourcesManager::CreateStorageBuffer(RENDERER_RESOURCE_NAME("Placements Entries Num Buffer"), entriesNumBufferSize, rhi::EMemoryUsage::GPUToCpu);

	exe.isFinished.store(true);
	exe.isProcessed.store(true);
}

void ComputePlacemenets(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const PlacementCommand& placementCommand, PlacementExe& exe)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(exe.isFinished.load());
	SPT_CHECK(!!exe.entries);
	SPT_CHECK(!!exe.entriesNum);
	SPT_CHECK(!!placementCommand.biome);
	SPT_CHECK(placementCommand.placementDefIdx < placementCommand.biome->placementDefinitions.size());

	const PlacementDefinition& placementDef = placementCommand.biome->placementDefinitions[placementCommand.placementDefIdx];

	const rg::RGBufferViewHandle entries    = graphBuilder.AcquireExternalBufferView(exe.entries->GetFullView());
	const rg::RGBufferViewHandle entriesNum = graphBuilder.AcquireExternalBufferView(exe.entriesNum->GetFullView());

	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Placements Count"), entriesNum, 0u);

	const math::Vector2i halfResolution = math::Vector2i::Constant(placementDef.resolution) / 2;
	const math::Vector2i beginCoords = (placementCommand.center / placementDef.placementSpacing).cast<Int32>() - halfResolution;

	PlacementConstants shaderConstants;
	shaderConstants.rwEntries         = entries;
	shaderConstants.rwEntriesNum      = entriesNum;
	shaderConstants.placementSpacing  = placementDef.placementSpacing;
	shaderConstants.resolution        = placementDef.resolution;
	shaderConstants.prefabsCollection = placementDef.prefabsCollection;
	shaderConstants.beginCoords       = beginCoords;
	shaderConstants.endCoords         = beginCoords + math::Vector2i::Constant(placementDef.resolution);

	if (placementCommand.lastCenter)
	{
		const math::Vector2i lastBeginCoords = (placementCommand.lastCenter.value() / placementDef.placementSpacing).cast<Int32>() - halfResolution;

		shaderConstants.lastBeginCoords  = lastBeginCoords;
		shaderConstants.lastEndCoords    = lastBeginCoords + math::Vector2i::Constant(placementDef.resolution);
		shaderConstants.lastCoordsValid  = true;
	}

	const Uint32 dispatchSize = math::Utils::DivideCeil(placementDef.resolution, 8u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute Placements"),
						  PlacementShaderPSO::pso,
						  math::Vector2u(dispatchSize, dispatchSize),
						  rg::EmptyDescriptorSets(),
						  shaderConstants);

	exe.placementDefIdx = placementCommand.placementDefIdx;

	exe.isFinished.store(false);
	exe.isProcessed.store(false);

	js::Launch("Finish Placements Processing",
			   [&exe]()
			   {
				   exe.isFinished.store(true);
			   },
			   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));
}

} // render_impl

PlacementSystemBacked::PlacementSystemBacked()
{

}

void PlacementSystemBacked::Initialize(lib::MemoryArena& arena, RenderScene& renderScene)
{
	(void) renderScene;

	for (PlacementExe*& exe : m_placementExecutions)
	{
		exe = arena.AllocateType<PlacementExe>();
		render_impl::InitializePlacementExe(*exe);
	}
}

void PlacementSystemBacked::Deinitialize(RenderScene& renderScene)
{
	for (PlacementExe*& exe : m_placementExecutions)
	{
		SPT_CHECK(exe->isFinished.load());

		exe->entries.reset();
		exe->entriesNum.reset();
		exe = nullptr;
	}
}

void PlacementSystemBacked::ProcessPlacements(PlacementProcessor processor, void* customData)
{
	process_impl::ProcessPlacementsResults(processor, customData, m_placementExecutions);
}

void PlacementSystemBacked::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	const Uint64 frameIdx = renderScene.GetCurrentFrameRef().GetFrameIdx();
	const Uint32 exeIdx = static_cast<Uint32>(frameIdx % m_placementExecutions.size());

	PlacementExe& exe = *m_placementExecutions[exeIdx];
	SPT_CHECK(exe.isFinished.load());
	SPT_CHECK(exe.isProcessed.load());

	const PlacementCommand& command = rendererInterface.rendererSettings.placementCommand;
	if (command.biome)
	{
		render_impl::ComputePlacemenets(graphBuilder, rendererInterface, renderScene, command, exe);
	}
}

} // spt::rsc
