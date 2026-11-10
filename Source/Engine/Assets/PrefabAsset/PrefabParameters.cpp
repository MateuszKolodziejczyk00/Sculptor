#include "PrefabParameters.h"
#include "PrefabParameterTypes.h"
#include "MaterialAsset.h"


SPT_DEFINE_LOG_CATEGORY(PrefabParameters, true);

namespace spt::as
{

using PrefabParameterTypesRegistry = lib::HashMap<lib::TypeID, PrefabParameterTypeMetaData>;


static PrefabParameterTypesRegistry& GetParameterTypesRegistry()
{
	static PrefabParameterTypesRegistry instance;
	return instance;
}


void RegisterPrefabParameterType(lib::TypeID type, const PrefabParameterTypeMetaData& metaData)
{
	GetParameterTypesRegistry()[type] = metaData;
}


const PrefabParameterTypeMetaData* GetPrefabParameterType(lib::TypeID type)
{
	const PrefabParameterTypesRegistry& registry = GetParameterTypesRegistry();
	const auto foundIt = registry.find(type);
	return foundIt != registry.cend() ? &foundIt->second : nullptr;
}


SPT_REGISTER_PREFAB_PARAMETER_TYPE(MaterialAsset);


struct PrefabBindingPath
{
	lib::HashedString::KeyType entity = {};
	lib::HashedString::KeyType member = {};
	Uint32                     elementIdx = 0u;
};


static std::optional<PrefabBindingPath> ParseBindingPath(lib::StringView path)
{
	// Parsed from the end, because entity names imported from GLTF often contain dots ("Cube.001_3")
	const SizeType bracketPos = path.rfind('[');
	if (bracketPos == lib::StringView::npos || path.back() != ']')
	{
		return std::nullopt;
	}

	const SizeType dotPos = path.rfind('.', bracketPos);
	if (dotPos == lib::StringView::npos || dotPos == 0u || dotPos + 1u == bracketPos)
	{
		return std::nullopt;
	}

	const char* elementIdxBegin = path.data() + bracketPos + 1u;
	const char* elementIdxEnd   = path.data() + path.size() - 1u;

	Uint32 elementIdx = 0u;
	const std::from_chars_result parseResult = std::from_chars(elementIdxBegin, elementIdxEnd, elementIdx);
	if (parseResult.ec != std::errc{} || parseResult.ptr != elementIdxEnd)
	{
		return std::nullopt;
	}

	return PrefabBindingPath
	{
		.entity     = lib::HashedString::HashString(path.substr(0u, dotPos)),
		.member     = lib::HashedString::HashString(path.substr(dotPos + 1u, bracketPos - dotPos - 1u)),
		.elementIdx = elementIdx
	};
}


Bool CompilePrefabParameters(PrefabAsset& prefab,
							 lib::Span<const PrefabParameterDefinition> definitions,
							 lib::Span<const PrefabEntityDefinitionEntry> entities,
							 lib::DynamicArray<CompiledPrefabParameter>& outParameters,
							 lib::DynamicArray<CompiledPrefabBinding>& outBindings)
{
	SPT_PROFILER_FUNCTION();

	struct NamedEntity
	{
		const PrefabEntityDefinition* definition = nullptr;
		Uint32                        compiledIdx = 0u;
	};

	lib::HashMap<lib::HashedString::KeyType, NamedEntity> namedEntities;

	Uint32 compiledEntitiesNum = 0u;
	for (const PrefabEntityDefinitionEntry& entry : entities)
	{
		// Null entries don't get compiled entity headers, so they must not advance compiled index
		if (entry.entity)
		{
			const NamedEntity namedEntity{ .definition = entry.entity.get(), .compiledIdx = compiledEntitiesNum++ };


			if (!entry.entity->name.GetView().empty() && !namedEntities.emplace(entry.entity->name.GetKey(), namedEntity).second)
			{
				SPT_LOG_ERROR(PrefabParameters, "Duplicated prefab entity name: {}", entry.entity->name.GetData());
				return false;
			}
		}
	}

	for (const PrefabParameterDefinition& definition : definitions)
	{
		const lib::HashedString::KeyType nameKey = lib::HashedString::HashString(definition.name);

		if (std::any_of(outParameters.cbegin(), outParameters.cend(), [nameKey](const CompiledPrefabParameter& parameter) { return parameter.nameKey == nameKey; }))
		{
			SPT_LOG_ERROR(PrefabParameters, "Duplicated prefab parameter name: {}", definition.name);
			return false;
		}

		CompiledPrefabParameter parameter{ .nameKey = nameKey };
		parameter.firstBindingIdx = static_cast<Uint32>(outBindings.size());

		for (const lib::String& binding : definition.bindings)
		{
			const std::optional<PrefabBindingPath> path = ParseBindingPath(binding);
			if (!path)
			{
				SPT_LOG_ERROR(PrefabParameters, "Prefab parameter {} has invalid binding path: {}. Expected format: Entity.member[idx]", definition.name, binding);
				return false;
			}

			const auto entityIt = namedEntities.find(path->entity);
			if (entityIt == namedEntities.cend())
			{
				SPT_LOG_ERROR(PrefabParameters, "Prefab parameter {} binding {} references missing entity", definition.name, binding);
				return false;
			}

			const as::PrefabParameterResolveContext resolveContext
			{
				.assetsSystem = prefab.GetOwningSystem(),
				.owningPrefab = prefab
			};

			const lib::TypeID bindingType = entityIt->second.definition->GetParameterType(resolveContext, path->member, path->elementIdx);
			if (bindingType == 0u)
			{
				SPT_LOG_ERROR(PrefabParameters, "Prefab parameter {} binding {} references member or element that can't be bound", definition.name, binding);
				return false;
			}

			SPT_CHECK_MSG(parameter.type == 0u || parameter.type == bindingType, "Prefab parameter {} has bindings with different types", definition.name);
			parameter.type = bindingType;
			parameter.bindingsNum++;

			outBindings.emplace_back(CompiledPrefabBinding
			{
				.entityIdx  = entityIt->second.compiledIdx,
				.elementIdx = path->elementIdx,
				.memberKey  = path->member
			});
		}

		outParameters.emplace_back(parameter);
	}

	return true;
}

} // spt::as
