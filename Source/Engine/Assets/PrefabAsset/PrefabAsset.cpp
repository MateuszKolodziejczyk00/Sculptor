#include "PrefabAsset.h"
#include "AssetsSystem.h"
#include "Importers/PrefabImporter.h"


SPT_DEFINE_LOG_CATEGORY(PrefabAsset, true);

namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompiledPrefab ================================================================================

struct PrefabDDCHeader
{
	Uint32 entitiesHeadersOffset = 0u;
	Uint32 entitiesNum           = 0u;

	Uint32 entitiesBlobOffset = 0u;
	Uint32 entitiesBlobSize   = 0u;

	Uint32 referencedAssetsNum    = 0u;
	Uint32 referencedAssetsOffset = 0u;

	Uint32 parametersNum    = 0u;
	Uint32 parametersOffset = 0u;

	Uint32 bindingsNum    = 0u;
	Uint32 bindingsOffset = 0u;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("EntitiesHeadersOffset",  entitiesHeadersOffset);
		serializer.Serialize("EntitiesNum",            entitiesNum);
		serializer.Serialize("EntitiesBlobOffset",     entitiesBlobOffset);
		serializer.Serialize("EntitiesBlobSize",       entitiesBlobSize);
		serializer.Serialize("ReferencedAssetsNum",    referencedAssetsNum);
		serializer.Serialize("ReferencedAssetsOffset", referencedAssetsOffset);
		serializer.Serialize("ParametersNum",          parametersNum);
		serializer.Serialize("ParametersOffset",       parametersOffset);
		serializer.Serialize("BindingsNum",            bindingsNum);
		serializer.Serialize("BindingsOffset",         bindingsOffset);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// PrefabCompiler ================================================================================

struct PrefabCompilationOutput
{
	lib::DynamicArray<CompiledPrefabEntityHeader> entityHeaders;
	lib::DynamicArray<Byte>                       compiledBlob;
	lib::DynamicArray<ResourcePathID>             referencedAssets;
	lib::DynamicArray<CompiledPrefabParameter>    parameters;
	lib::DynamicArray<CompiledPrefabBinding>      bindings;
};


struct PrefabCompiler
{
	const PrefabAsset& owningPrefab;
	PrefabCompilationOutput& output;
};

namespace prefab_compiler_api
{

ResourcePathID CreateAssetDependency(PrefabCompiler& compiler, const lib::Path& relativePath)
{
	const ResourcePath path = compiler.owningPrefab.ResolveAssetRelativePath(relativePath);
	const Bool isValidAsset = compiler.owningPrefab.GetOwningSystem().CompileAssetIfDeprecated(path);
	if (isValidAsset)
	{
		const ResourcePathID pathID = path.GetID();
		if (!lib::Contains(compiler.output.referencedAssets, pathID))
		{
			compiler.output.referencedAssets.emplace_back(pathID);
		}

		return pathID;
	}
	else
	{
		return InvalidResourcePathID;
	}
}

void CompilePrefabEntity(PrefabCompiler& compiler, const PrefabEntityDefinitionEntry& definition)
{
	if (definition.entity)
	{
		const lib::DynamicArray<Byte> compiledEntityBlob = definition.entity->Compile(compiler);

		compiler.output.entityHeaders.emplace_back(CompiledPrefabEntityHeader
		{
			.type        = definition.entity->GetType(),
			.dataOfffset = static_cast<Uint32>(compiler.output.compiledBlob.size()),
			.dataSize    = static_cast<Uint32>(compiledEntityBlob.size())
		});

		compiler.output.compiledBlob.insert(compiler.output.compiledBlob.end(), compiledEntityBlob.cbegin(), compiledEntityBlob.cend());
	}
}

} // prefab_compiler_api

//////////////////////////////////////////////////////////////////////////////////////////////////
// PrefabDataInitializer =========================================================================

void PrefabDataInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<PrefabDefinition>(std::move(m_definition));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GLTFPrefabDataInitializer =====================================================================

void GLTFPrefabDataInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<GLTFPrefabDefinition>(std::move(m_definition));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PrefabAsset ===================================================================================

void PrefabAsset::Spawn(void* context)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsInitialized());

	for (Uint32 entityIdx = 0u; entityIdx < m_entities.size(); ++entityIdx)
	{
		const CompiledPrefabEntityHeader& entity = m_entities[entityIdx];
		if (const PrefabEntityTypeMetaData* typeMetaData = GetPrefabType(entity.type))
		{
			typeMetaData->spawner(context, entityIdx, m_entitiesData.subspan(entity.dataOfffset, entity.dataSize));
		}
	}
}

void PrefabAsset::SetParameter(const void* context, lib::HashedString::KeyType nameKey, const PrefabParameterPayload& payload) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsInitialized());

	const auto parameterIt = std::find_if(m_parameters.cbegin(), m_parameters.cend(), [nameKey](const CompiledPrefabParameter& parameter) { return parameter.nameKey == nameKey; });

	SPT_CHECK_MSG(parameterIt != m_parameters.cend(), "Prefab parameter {} doesn't exist", nameKey);
	SPT_CHECK_MSG(parameterIt->type == payload.type, "Invalid value type for prefab parameter {}", nameKey);

	const Uint32 firstBindingIdx = parameterIt->firstBindingIdx;
	const Uint32 bindingsEndIdx = firstBindingIdx + parameterIt->bindingsNum;

	for (Uint32 bindingIdx = firstBindingIdx; bindingIdx < bindingsEndIdx; ++bindingIdx)
	{
		const CompiledPrefabBinding& binding = m_bindings[bindingIdx];

		const PrefabEntityTypeMetaData* typeMetaData = GetPrefabType(m_entities[binding.entityIdx].type);
		SPT_CHECK(!!typeMetaData && typeMetaData->applier.IsValid());

		typeMetaData->applier(context, binding, payload);
	}
}

void PrefabAsset::SetParameter(const void* context, lib::StringView name, const PrefabParameterPayload& payload) const
{
	SPT_PROFILER_FUNCTION();

	SetParameter(context, lib::HashedString::HashString(name), payload);
}

lib::TypeID PrefabAsset::GetParameterType(lib::HashedString::KeyType paramName) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsInitialized());

	const auto parameterIt = std::find_if(m_parameters.cbegin(), m_parameters.cend(), [paramName](const CompiledPrefabParameter& parameter) { return parameter.nameKey == paramName; });

	return (parameterIt != m_parameters.cend()) ? parameterIt->type : lib::TypeID{};
}

Bool PrefabAsset::Compile()
{
	SPT_PROFILER_FUNCTION();

	if (const PrefabDefinition* prefabDef = GetBlackboard().Find<PrefabDefinition>())
	{
		return CompileDefinition(*prefabDef);
	}
	else if (const GLTFPrefabDefinition* prefabSourceDef = GetBlackboard().Find<GLTFPrefabDefinition>())
	{
		PrefabDefinition importedDef;
		importedDef.parameters = prefabSourceDef->parameters;

		const lib::Path sourcePath = GetDirectoryPath() / prefabSourceDef->gltfSourcePath;
		importer::ImportPrefabDefinition(GetOwningSystem(), sourcePath, OUT importedDef);
		return CompileDefinition(importedDef);
	}
	else
	{
		return false;
	}
}

void PrefabAsset::OnInitialize()
{
	SPT_PROFILER_FUNCTION();

	const lib::MTHandle<DDCLoadedData<PrefabDDCHeader>> dd = LoadDerivedData<PrefabDDCHeader>(*this);

	lib::Span<const ResourcePathID> referencedAssets(reinterpret_cast<const ResourcePathID*>(dd->bin.data() + dd->header.referencedAssetsOffset), dd->header.referencedAssetsNum);
	m_referencedAssets.reserve(referencedAssets.size());
	for (const ResourcePathID referencedAsset : referencedAssets)
	{
		const LoadResult loadRes = GetOwningSystem().LoadAsset(referencedAsset);
		if (loadRes)
		{
			m_referencedAssets.emplace_back(loadRes.GetValue());
		}
		else
		{
			SPT_LOG_ERROR(PrefabAsset, "Failed to load referenced asset with path ID {}. Reason: {}", referencedAsset, AssetLoadErrorToString(loadRes.GetError()));
		}
	}

	const Uint32 runtimeDataSize = dd->header.entitiesNum * sizeof(CompiledPrefabEntityHeader) + dd->header.entitiesBlobSize;
	m_compiledBlob.resize(runtimeDataSize);
	std::memcpy(m_compiledBlob.data(), dd->bin.data(), runtimeDataSize);
	m_entities     = lib::Span<const CompiledPrefabEntityHeader>(reinterpret_cast<const CompiledPrefabEntityHeader*>(m_compiledBlob.data()), dd->header.entitiesNum);
	m_entitiesData = lib::Span<const Byte>(m_compiledBlob.data() + m_entities.size() * sizeof(CompiledPrefabEntityHeader), dd->header.entitiesBlobSize);

	m_parameters.resize(dd->header.parametersNum);
	std::memcpy(m_parameters.data(), dd->bin.data() + dd->header.parametersOffset, m_parameters.size() * sizeof(CompiledPrefabParameter));

	m_bindings.resize(dd->header.bindingsNum);
	memcpy(m_bindings.data(), dd->bin.data() + dd->header.bindingsOffset, m_bindings.size() * sizeof(CompiledPrefabBinding));

	for (const AssetHandle& referencedAsset : m_referencedAssets)
	{
		referencedAsset->AwaitInitialization();
	}
}

Bool PrefabAsset::CompileDefinition(const PrefabDefinition& def)
{
	SPT_PROFILER_FUNCTION();

	PrefabCompilationOutput compilationOutput;
	PrefabCompiler compiler
	{
		.owningPrefab = *this,
		.output       = compilationOutput
	};

	for (const PrefabEntityDefinitionEntry& entityDef : def.entities)
	{
		prefab_compiler_api::CompilePrefabEntity(compiler, entityDef);
	}

	if (!CompilePrefabParameters(*this, def.parameters, def.entities, OUT compilationOutput.parameters, OUT compilationOutput.bindings))
	{
		return false;
	}

	const Uint32 entityHeadersOffset    = 0u;
	const Uint32 entityHeadersSize      = static_cast<Uint32>(compilationOutput.entityHeaders.size() * sizeof(CompiledPrefabEntityHeader));
	const Uint32 entitiesBlobOffset     = entityHeadersOffset + entityHeadersSize;
	const Uint32 entitiesBlobSize       = static_cast<Uint32>(compilationOutput.compiledBlob.size());
	const Uint32 referencedAssetsOffset = entitiesBlobOffset + entitiesBlobSize;
	const Uint32 referencedAssetsSize   = static_cast<Uint32>(compilationOutput.referencedAssets.size() * sizeof(ResourcePathID));

	const Uint32 parametersOffset        = referencedAssetsOffset + referencedAssetsSize;
	const Uint32 parametersSize          = static_cast<Uint32>(compilationOutput.parameters.size() * sizeof(CompiledPrefabParameter));

	const Uint32 bindingsOffset        = parametersOffset + parametersSize;
	const Uint32 bindingsSize          = static_cast<Uint32>(compilationOutput.bindings.size() * sizeof(CompiledPrefabBinding));

	const Uint32 blobSize = bindingsOffset + bindingsSize;

	const PrefabDDCHeader ddcHeader
	{
		.entitiesHeadersOffset  = entityHeadersOffset,
		.entitiesNum            = static_cast<Uint32>(compilationOutput.entityHeaders.size()),
		.entitiesBlobOffset     = entitiesBlobOffset,
		.entitiesBlobSize       = entitiesBlobSize,
		.referencedAssetsNum    = static_cast<Uint32>(compilationOutput.referencedAssets.size()),
		.referencedAssetsOffset = referencedAssetsOffset,
		.parametersNum           = static_cast<Uint32>(compilationOutput.parameters.size()),
		.parametersOffset        = parametersOffset,
		.bindingsNum             = static_cast<Uint32>(compilationOutput.bindings.size()),
		.bindingsOffset          = bindingsOffset
	};

	CreateDerivedData(*this, ddcHeader, blobSize,
			[&](lib::Span<Byte> outData)
			{
				std::memcpy(outData.data() + entityHeadersOffset,    compilationOutput.entityHeaders.data(), entityHeadersSize);
				std::memcpy(outData.data() + entitiesBlobOffset,     compilationOutput.compiledBlob.data(), entitiesBlobSize);
				std::memcpy(outData.data() + referencedAssetsOffset, compilationOutput.referencedAssets.data(), referencedAssetsSize);
				std::memcpy(outData.data() + parametersOffset, compilationOutput.parameters.data(), parametersSize);
				std::memcpy(outData.data() + bindingsOffset, compilationOutput.bindings.data(), bindingsSize);
			});

	return true;
}

} // spt::as
