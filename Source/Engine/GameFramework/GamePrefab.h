#pragma once

#include "Entities/PrefabEntity.h"
#include "GameFrameworkTypes.h"


namespace spt::gf
{

class World;
struct PrefabInstance;


struct SpawnContext
{
	as::AssetsSystem& assetsSystem;
	World&            world;
	PrefabInstance&   prefabInstance;

	Transform         transform;
};


struct GamePrefabEntityDefinition : public as::PrefabEntityDefinition
{
	template<typename TEntityType>
	static as::PrefabEntitySpawner RegisterSpawner()
	{
		return as::PrefabEntitySpawner([](const void* context, lib::Span<const Byte> compiledData)
		{
			const SpawnContext& spawnContext = *static_cast<const SpawnContext*>(context);
			TEntityType::Spawn(spawnContext, compiledData);
		});
	}
	
	static void Spawn(const SpawnContext& context, lib::Span<const Byte> compiledData) {}
};

} // spt::gf w w
