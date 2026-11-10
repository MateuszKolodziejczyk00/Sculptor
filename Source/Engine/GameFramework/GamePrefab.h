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

	const Transform   transform;
};


struct SetParameterContext
{
	World&                world;
	const PrefabInstance& prefabInstance;
};


struct GamePrefabEntityDefinition : public as::PrefabEntityDefinition
{
	template<typename TEntityType>
	static as::PrefabEntitySpawner RegisterSpawner()
	{
		return as::PrefabEntitySpawner([](void* context, Uint32 entityIdx, lib::Span<const Byte> compiledData)
		{
			SpawnContext& spawnContext = *static_cast<SpawnContext*>(context);
			TEntityType::Spawn(spawnContext, entityIdx, compiledData);
		});
	}

	template<typename TEntityType>
	static as::PrefabEntityParameterApplier RegisterParameterApplier()
	{
		return as::PrefabEntityParameterApplier([](const void* context, const as::CompiledPrefabBinding& binding, const as::PrefabParameterPayload& payload)
		{
			TEntityType::ApplyParameter(*static_cast<const SetParameterContext*>(context), binding, payload);
		});
	}

	static void Spawn(SpawnContext& context, Uint32 entityIdx, lib::Span<const Byte> compiledData) {}

	static void ApplyParameter(const SetParameterContext& context, const as::CompiledPrefabBinding& binding, const as::PrefabParameterPayload& payload) {}
};

} // spt::gf w w
