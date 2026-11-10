#pragma once

#include "SculptorCoreTypes.h"
#include "Serialization.h"


namespace spt::as
{

struct PrefabEntityDefinitionEntry;
class PrefabAsset;


struct PrefabParameterDefinition
{
	lib::String                    name;
	lib::DynamicArray<lib::String> bindings;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Name",     name);
		serializer.Serialize("Bindings", bindings);
	}
};


struct CompiledPrefabBinding
{
	Uint32                     entityIdx  = 0u;
	Uint32                     elementIdx = 0u;
	lib::HashedString::KeyType memberKey  = 0u;
};


struct CompiledPrefabParameter
{
	lib::HashedString::KeyType nameKey = 0u;
	lib::TypeID                type    = 0u;

	Uint32 firstBindingIdx = 0u;
	Uint32 bindingsNum     = 0u;
};


struct CompiledPrefabOverride
{
	lib::HashedString::KeyType paramKey   = 0u;
	lib::TypeID                valueType  = 0u;
	Uint32                     dataOffset = 0u;
	Uint32                     dataSize   = 0u;
};

// Non-owning, valid only during the call
struct PrefabParameterPayload
{
	lib::TypeID type = 0u;
	const void* data = nullptr;
};


Bool CompilePrefabParameters(PrefabAsset& prefab,
							 lib::Span<const PrefabParameterDefinition> definitions,
							 lib::Span<const PrefabEntityDefinitionEntry> entities,
							 lib::DynamicArray<CompiledPrefabParameter>& outParameters,
							 lib::DynamicArray<CompiledPrefabBinding>& outBindings);

} // spt::as
