#pragma once

#include "Entities/PrefabEntity.h"
#include "GamePrefab.h"
#include "PrefabParameterTypes.h"


namespace spt::gf
{

struct PrefabChildEntityDefinition : public GamePrefabEntityDefinition
{
	lib::Path prefab;

	lib::DynamicArray<as::PrefabParameterOverride> overrides;

	virtual lib::RuntimeTypeInfo    GetType() const override { return lib::TypeInfo<PrefabChildEntityDefinition>(); }
	virtual lib::DynamicArray<Byte> Compile(as::PrefabCompiler& compiler) const override;
	virtual lib::TypeID             GetParameterType(const as::PrefabParameterResolveContext& context, lib::HashedString::KeyType member, Uint32 elementIdx) const override;

	static void Spawn(SpawnContext& context, Uint32 entityIdx, lib::Span<const Byte> compiledData);
	static void ApplyParameter(const SetParameterContext& context, const as::CompiledPrefabBinding& binding, const as::PrefabParameterPayload& payload);

	virtual void Serialize(srl::Serializer& serializer) override
	{
		PrefabEntityDefinition::Serialize(serializer);

		serializer.Serialize("Prefab",    prefab);
		serializer.Serialize("Overrides", overrides);
	}
};
SPT_REGISTER_PREFAB_ENTITY_TYPE(PrefabChildEntityDefinition);

} // spt::gf
