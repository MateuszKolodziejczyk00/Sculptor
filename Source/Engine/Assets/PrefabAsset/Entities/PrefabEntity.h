#pragma once

#include "PrefabAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "Serialization.h"
#include "Utility/Templates/Callable.h"


namespace spt::rsc
{
class RenderScene;
} // spt::rsc


namespace spt::as
{

class AssetsSystem;
struct PrefabCompiler;


struct PrefabSpawningContext
{
	AssetsSystem& assetsSystem;
	rsc::RenderScene& scene;
	math::Affine3f transform;
};


struct PrefabEntity
{
	virtual ~PrefabEntity() = default;

	math::Vector3f location = math::Vector3f::Zero();
	math::Vector3f rotation = math::Vector3f::Zero();
	math::Vector3f scale    = math::Vector3f::Ones();

	virtual lib::RuntimeTypeInfo    GetType() const = 0;
	virtual lib::DynamicArray<Byte> Compile(PrefabCompiler& compiler) const = 0;

	static void Spawn(const PrefabSpawningContext& context, lib::Span<const Byte> compiledData) {}

	virtual void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Location", location);
		serializer.Serialize("Rotation", rotation);
		serializer.Serialize("scale",    scale);
	}
};


using PrefabEntityFactory = lib::RawCallable<lib::UniquePtr<PrefabEntity>()>;
using PrefabEntitySpawner = lib::RawCallable<void(const PrefabSpawningContext& /* context */, lib::Span<const Byte> /* compiledData */)>;


struct PrefabEntityTypeMetaData
{
	PrefabEntityFactory factory;
	PrefabEntitySpawner spawner;
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
			.factory = []{ return lib::UniquePtr<PrefabEntity>{ new TInstanceType() }; },
			.spawner = [](const PrefabSpawningContext& context, lib::Span<const Byte> compiledData) { TInstanceType::Spawn(context, compiledData); }
		};
		RegisterPrefabType(lib::TypeInfo<TInstanceType>(), typeMetaData);
	}
};
#define SPT_REGISTER_PREFAB_ENTITY_TYPE(type) \
static spt::as::PrefabEntityTypeRegistrator<type> SPT_SCOPE_NAME_EVAL(autoRegistrator, __LINE__);


} // spt::as
