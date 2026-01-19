#pragma once

#include "PrefabAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "Entities/PrefabEntity.h"


namespace spt::as
{

struct PrefabCompiler;


namespace prefab_compiler_api
{

ResourcePathID PREFAB_ASSET_API CreateAssetDependency(PrefabCompiler& compiler, const lib::Path& relativePath);

} // prefab_compiler_api


struct PrefabEntityDefinition
{
	lib::UniquePtr<PrefabEntity> entity;

	void Serialize(srl::Serializer& serializer)
	{
		if (serializer.IsSaving())
		{
			lib::RuntimeTypeInfo type = entity ? entity->GetType() : lib::RuntimeTypeInfo();
			serializer.Serialize("EntityType", type);
		}
		else
		{
			lib::RuntimeTypeInfo entityType;
			serializer.Serialize("EntityType", entityType);

			if(const PrefabEntityTypeMetaData* typeMetaData = GetPrefabType(entityType))
			{
				entity = typeMetaData->factory();
			}
		}

		if (entity)
		{
			serializer.Serialize("Entity", *entity);
		}
	}
};


struct PrefabDefinition
{
	lib::DynamicArray<PrefabEntityDefinition> entities;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Entities", entities);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(PrefabDefinition);


class PREFAB_ASSET_API PrefabDataInitializer : public AssetDataInitializer
{
public:

	explicit PrefabDataInitializer(PrefabDefinition&& definition)
		: m_definition(std::move(definition))
	{
	}

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	PrefabDefinition m_definition;
};


struct GLTFPrefabDefinition
{
	lib::Path gltfSourcePath;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("GLTFSourcePath", gltfSourcePath);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(GLTFPrefabDefinition);


class PREFAB_ASSET_API GLTFPrefabDataInitializer : public AssetDataInitializer
{
public:

	explicit GLTFPrefabDataInitializer(GLTFPrefabDefinition definition)
		: m_definition(std::move(definition))
	{
	}

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	GLTFPrefabDefinition m_definition;
};


struct CompiledPrefabEntityHeader
{
	lib::RuntimeTypeInfo type;
	Uint32 dataOfffset = 0u;
	Uint32 dataSize    = 0u;
};


struct PrefabSpawnParams
{
	rsc::RenderScene& scene;
	math::Vector3f location = math::Vector3f::Zero();
	math::Vector3f rotation = math::Vector3f::Zero();
	math::Vector3f scale    = math::Vector3f::Ones();
};


class PREFAB_ASSET_API PrefabAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(PrefabAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	void Spawn(const PrefabSpawnParams& params);

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	Bool CompileDefinition(const PrefabDefinition& def);

	lib::DynamicArray<Byte> m_compiledBlob;
	lib::Span<const CompiledPrefabEntityHeader> m_entities;
	lib::Span<const Byte>                       m_entitiesData;

	lib::DynamicArray<AssetHandle> m_referencedAssets;
};
SPT_REGISTER_ASSET_TYPE(PrefabAsset);

} // spt::as
