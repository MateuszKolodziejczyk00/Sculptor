#include "WorldPlacementSystem.h"
#include "PlacementSystem/PlacementSystemTypes.h"
#include "SceneRenderer/SceneRenderer.h"
#include "World.h"


namespace spt::gf
{

void WorldPlacementSystem::SetBiome(const rsc::BiomeDefinition& biome, lib::DynamicArray<as::PrefabAssetHandle> placementAssets)
{
	m_biome           = biome;
	m_placementAssets = std::move(placementAssets);

	m_placementStates.resize(m_biome.placementDefinitions.size());
	for (Uint32 defIdx = 0u; defIdx < m_biome.placementDefinitions.size(); ++defIdx)
	{
		const rsc::PlacementDefinition& placementDef = m_biome.placementDefinitions[defIdx];
		PlacementState& placementState = m_placementStates[defIdx];

		placementState.entries.resize(placementDef.resolution * placementDef.resolution);
	}
}

void WorldPlacementSystem::ProcessPlacements(gf::World& world, rsc::SceneRendererHandle sceneRenderer)
{
	SPT_PROFILER_FUNCTION();

	const SceneRendererDLLModuleAPI* sceneRendererAPI = engn::Engine::Get().GetModulesManager().GetModuleAPI<SceneRendererDLLModuleAPI>();

	struct PlacementData
	{
		gf::World&            world;
		WorldPlacementSystem& placementSystem;
	};

	PlacementData placementData{ world, *this };

	rsc::PlacementProcessor processor([](void* customData, const rsc::PlacementProcessData& processData)
	{
		const PlacementData& data = *reinterpret_cast<PlacementData*>(customData);

		PlacementState& placementState = data.placementSystem.m_placementStates[processData.placementDefIdx];

		for (const rsc::PlacementEntry& placement : processData.placements)
		{
			PlacementEntryState& entryState = placementState.entries[placement.entryIdx];
			if (entryState.prefabInstance.IsValid())
			{
				data.world.DestroyPrefabInstance(placementState.entries[placement.entryIdx].prefabInstance);
				entryState.prefabInstance = PrefabInstanceHandle{};
			}

			if (placement.prefabIdx != idxNone<Uint32>)
			{
				const as::PrefabAssetHandle& prefabHandle = data.placementSystem.m_placementAssets[placement.prefabIdx];
				entryState.prefabInstance = data.world.SpawnPrefab(prefabHandle,
																   gf::PrefabSpawnParams
																   {
																	   .location = placement.location,
																	   .scale = math::Vector3f::Constant(placement.scale),
																   });
			}
		}
	});

	sceneRendererAPI->ProcessPlacements(sceneRenderer, processor, &placementData);
}

rsc::PlacementCommand WorldPlacementSystem::CreatePlacementCommand(engn::FrameContext& frame, math::Vector3f location)
{
	if (m_biome.placementDefinitions.empty())
	{
		return rsc::PlacementCommand{};
	}

	const Uint32 placementIdx = static_cast<Uint32>(frame.GetFrameIdx() % m_biome.placementDefinitions.size());

	rsc::PlacementCommand placementCommand;
	placementCommand.biome           = &m_biome;
	placementCommand.placementDefIdx = placementIdx;
	placementCommand.center          = location.head<2>();
	placementCommand.lastCenter      = m_placementStates[placementIdx].lastCommandLocation;

	m_placementStates[placementIdx].lastCommandLocation = placementCommand.center;

	return placementCommand;
}

} // spt::gf
