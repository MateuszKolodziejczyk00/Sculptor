#pragma once

#include "PrefabAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "Serialization.h"
#include "PrefabParameters.h"
#include "Utility/Templates/Callable.h"


namespace spt::rsc
{
class RenderScene;
} // spt::rsc


namespace spt::as
{

class AssetsSystem;
class PrefabAsset;
struct PrefabCompiler;


struct PrefabSpawningContext
{
	AssetsSystem& assetsSystem;
	rsc::RenderScene& scene;
	math::Affine3f transform;
};


using PrefabEntitySpawner = lib::RawCallable<void(void* context, Uint32 /* entityIdx */, lib::Span<const Byte> /* compiledData */)>;

using PrefabEntityParameterApplier = lib::RawCallable<void(const void* context, const CompiledPrefabBinding& /* binding */, const PrefabParameterPayload& /* payload */)>;


struct PrefabParameterResolveContext
{
	AssetsSystem& assetsSystem;
	PrefabAsset&  owningPrefab;
};


struct PrefabEntityDefinition
{
	virtual ~PrefabEntityDefinition() = default;

	lib::HashedString name;

	math::Vector3f location = math::Vector3f::Zero();
	math::Vector3f rotation = math::Vector3f::Zero();
	math::Vector3f scale    = math::Vector3f::Ones();

	virtual lib::RuntimeTypeInfo    GetType() const = 0;
	virtual lib::DynamicArray<Byte> Compile(PrefabCompiler& compiler) const = 0;

	virtual lib::TypeID GetParameterType(const PrefabParameterResolveContext& context, lib::HashedString::KeyType member, Uint32 elementIdx) const { return lib::TypeID{}; }

	template<typename TEntityType>
	static PrefabEntitySpawner RegisterSpawner() { return PrefabEntitySpawner{}; }

	template<typename TEntityType>
	static PrefabEntityParameterApplier RegisterParameterApplier() { return PrefabEntityParameterApplier{}; }

	virtual void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Name",     name);
		serializer.Serialize("Location", location);
		serializer.Serialize("Rotation", rotation);
		serializer.Serialize("scale",    scale);
	}
};


using PrefabEntityFactory = lib::RawCallable<lib::UniquePtr<PrefabEntityDefinition>()>;


struct PrefabEntityTypeMetaData
{
	PrefabEntityFactory factory;
	PrefabEntitySpawner spawner;
	PrefabEntityParameterApplier applier;
};


void                            PREFAB_ASSET_API RegisterPrefabType(const lib::RuntimeTypeInfo& type, const PrefabEntityTypeMetaData& factory);
const PrefabEntityTypeMetaData* PREFAB_ASSET_API GetPrefabType(const lib::RuntimeTypeInfo& type);


template<typename TInstanceType>
struct PrefabEntityTypeRegistrator
{
	PrefabEntityTypeRegistrator()
	{
		const PrefabEntityTypeMetaData typeMetaData
		{
			.factory = []{ return lib::UniquePtr<PrefabEntityDefinition>{ new TInstanceType() }; },
			.spawner = TInstanceType::template RegisterSpawner<TInstanceType>(),
			.applier = TInstanceType::template RegisterParameterApplier<TInstanceType>()
		};
		RegisterPrefabType(lib::TypeInfo<TInstanceType>(), typeMetaData);
	}
};
#define SPT_REGISTER_PREFAB_ENTITY_TYPE(type) \
static spt::as::PrefabEntityTypeRegistrator<type> SPT_IDENTIFIER_NAME(autoRegistrator);


} // spt::as
