#include "PrefabAsset.h"
#include "AssetsSystem.h"
#include "Importers/GLTFImporter.h"


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

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("EntitiesHeadersOffset",  entitiesHeadersOffset);
		serializer.Serialize("EntitiesNum",            entitiesNum);
		serializer.Serialize("EntitiesBlobOffset",     entitiesBlobOffset);
		serializer.Serialize("EntitiesBlobSize",       entitiesBlobSize);
		serializer.Serialize("ReferencedAssetsNum",    referencedAssetsNum);
		serializer.Serialize("ReferencedAssetsOffset", referencedAssetsOffset);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// PrefabCompiler ================================================================================

struct PrefabCompilationOutput
{
	lib::DynamicArray<CompiledPrefabEntityHeader> entityHeaders;
	lib::DynamicArray<Byte>                       compiledBlob;
	lib::DynamicArray<ResourcePathID>             referencedAssets;
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

void CompilePrefabEntity(PrefabCompiler& compiler, const PrefabEntityDefinition& definition)
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

void PrefabAsset::Spawn(const PrefabSpawnParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsInitialized());

	math::Affine3f transform = math::Affine3f::Identity();
	transform.prescale(params.scale);
	transform.prerotate(math::Utils::EulerToQuaternionDegrees(params.rotation.x(), params.rotation.y(), params.rotation.z()));
	transform.pretranslate(params.location);

	const PrefabSpawningContext spawningContext
	{
		.assetsSystem = GetOwningSystem(),
		.scene        = params.scene,
		.transform    = transform
	};

	for (const CompiledPrefabEntityHeader& entity : m_entities)
	{
		if (const PrefabEntityTypeMetaData* typeMetaData = GetPrefabType(entity.type))
		{
			typeMetaData->spawner(spawningContext, m_entitiesData.subspan(entity.dataOfffset, entity.dataSize));
		}
	}
}

Bool PrefabAsset::Compile()
{
	SPT_PROFILER_FUNCTION();

	if (const PrefabDefinition* prefabDef = GetBlackboard().Find<PrefabDefinition>())
	{
		return CompileDefinition(*prefabDef);
	}
	else if (const GLTFPrefabDefinition* gltfPrefabDef = GetBlackboard().Find<GLTFPrefabDefinition>())
	{
		PrefabDefinition importedDef;
		importer::InitPrefabDefinition(importedDef, *this, *gltfPrefabDef);
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

	for (const PrefabEntityDefinition& entityDef : def.entities)
	{
		prefab_compiler_api::CompilePrefabEntity(compiler, entityDef);
	}

	const Uint32 entityHeadersOffset    = 0u;
	const Uint32 entityHeadersSize      = static_cast<Uint32>(compilationOutput.entityHeaders.size() * sizeof(CompiledPrefabEntityHeader));
	const Uint32 entitiesBlobOffset     = entityHeadersOffset + entityHeadersSize;
	const Uint32 entitiesBlobSize       = static_cast<Uint32>(compilationOutput.compiledBlob.size());
	const Uint32 referencedAssetsOffset = entitiesBlobOffset + entitiesBlobSize;
	const Uint32 referencedAssetsSize   = static_cast<Uint32>(compilationOutput.referencedAssets.size() * sizeof(ResourcePathID));

	const Uint32 blobSize = referencedAssetsOffset + referencedAssetsSize;

	const PrefabDDCHeader ddcHeader
	{
		.entitiesHeadersOffset  = entityHeadersOffset,
		.entitiesNum            = static_cast<Uint32>(compilationOutput.entityHeaders.size()),
		.entitiesBlobOffset     = entitiesBlobOffset,
		.entitiesBlobSize       = entitiesBlobSize,
		.referencedAssetsNum    = static_cast<Uint32>(compilationOutput.referencedAssets.size()),
		.referencedAssetsOffset = referencedAssetsOffset,
	};

	CreateDerivedData(*this, ddcHeader, blobSize,
			[&](lib::Span<Byte> outData)
			{
				std::memcpy(outData.data() + entityHeadersOffset,    compilationOutput.entityHeaders.data(), entityHeadersSize);
				std::memcpy(outData.data() + entitiesBlobOffset,     compilationOutput.compiledBlob.data(), entitiesBlobSize);
				std::memcpy(outData.data() + referencedAssetsOffset, compilationOutput.referencedAssets.data(), referencedAssetsSize);
			});

	return true;
}

} // spt::as
