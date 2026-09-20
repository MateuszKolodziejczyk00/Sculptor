#pragma once

#include "Entities/PrefabEntity.h"
#include "GameFrameworkTypes.h"


namespace spt::gf
{

class World;
struct PrefabInstance;
struct MeshesChunk;


struct SpawnContext
{
	as::AssetsSystem& assetsSystem;
	World&            world;
	PrefabInstance&   prefabInstance;

	const Transform   transform;

	MeshesChunk*      lastMeshesChunk = nullptr;
};


struct GamePrefabEntityDefinition : public as::PrefabEntityDefinition
{
	template<typename TEntityType>
	static as::PrefabEntitySpawner RegisterSpawner()
	{
		return as::PrefabEntitySpawner([](void* context, lib::Span<const Byte> compiledData)
		{
			SpawnContext& spawnContext = *static_cast<SpawnContext*>(context);
			TEntityType::Spawn(spawnContext, compiledData);
		});
	}
	
	static void Spawn(SpawnContext& context, lib::Span<const Byte> compiledData) {}
};

} // spt::gf w w
