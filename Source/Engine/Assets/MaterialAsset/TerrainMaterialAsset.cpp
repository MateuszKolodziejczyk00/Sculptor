#include "TerrainMaterialAsset.h"
#include "AssetsSystem.h"


SPT_DEFINE_LOG_CATEGORY(TerrainMaterialAsset, true);

namespace spt::as
{

struct TerrainCompiledMaterialEntry
{
	ResourcePathID materialAssetPathID = InvalidResourcePathID;
	Real32         uvScale             = 1.f;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MaterialAssetPathID", materialAssetPathID);
		serializer.Serialize("UVScale",             uvScale);
	}
};


struct TerrainMaterialDerivedDataHeader
{
	lib::StaticArray<TerrainCompiledMaterialEntry, rsc::terrain_material_props::maxMaterialEntries> compiledEntries;

	TerrainMaterialDerivedDataHeader()
	{
	}

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("CompiledEntries", compiledEntries);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// TerrainMaterialInitializer ====================================================================

void TerrainMaterialInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<TerrainMaterialDefinition>(std::move(m_definition));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TerrainMaterialAsset ===========================================================================

const rsc::TerrainMaterialData& TerrainMaterialAsset::GetTerrainMaterialData() const
{
	return m_terrainMaterialData;
}

Bool TerrainMaterialAsset::Compile()
{
	const TerrainMaterialDefinition& definition = GetBlackboard().Get<TerrainMaterialDefinition>();

	TerrainMaterialDerivedDataHeader header;

	for (SizeType i = 0; i < definition.materialEntries.GetSize(); ++i)
	{
		const TerrainMaterialEntry& materialEntry = definition.materialEntries[i];
		if (!materialEntry.materialAsset.IsValid())
		{
			continue;
		}

		const ResourcePath materialAssetPath = ResolveAssetRelativePath(materialEntry.materialAsset.GetPath());
		if (!GetOwningSystem().CompileAssetIfDeprecated(materialAssetPath))
		{
			SPT_LOG_ERROR(TerrainMaterialAsset, "Failed to compile TerrainMaterialAsset '{}' - referenced MaterialAsset '{}' could not be compiled", GetName().ToString(), materialAssetPath.GetPath().generic_string());
			return false;
		}

		TerrainCompiledMaterialEntry& compiledEntry = header.compiledEntries[i];
		compiledEntry.materialAssetPathID = materialAssetPath.GetID();
		compiledEntry.uvScale             = 1.f / materialEntry.uvTileMeters;
	}

	CreateDerivedData(*this, header, lib::Span<const Byte>());

	return true;
}

void TerrainMaterialAsset::OnInitialize()
{
	const lib::MTHandle<DDCLoadedData<TerrainMaterialDerivedDataHeader>> compiledData = LoadDerivedData<TerrainMaterialDerivedDataHeader>(*this);
	SPT_CHECK(compiledData.IsValid());

	for (SizeType i = 0; i < compiledData->header.compiledEntries.size(); ++i)
	{
		const TerrainCompiledMaterialEntry& compiledEntry = compiledData->header.compiledEntries[i];
		const ResourcePathID materialAssetPathID = compiledEntry.materialAssetPathID;
		if (materialAssetPathID != InvalidResourcePathID)
		{
			const LoadResult<MaterialAsset> loadRes = GetOwningSystem().LoadAsset<MaterialAsset>(materialAssetPathID);
			if (!loadRes)
			{
				SPT_LOG_ERROR(TerrainMaterialAsset, "Failed to load referenced MaterialAsset for TerrainMaterialAsset '{}'. Reason: {}", GetName().ToString(), AssetLoadErrorToString(loadRes.GetError()));
				continue;
			}

			m_materialAssets.PushBack(loadRes.GetValue());
		}
	}

	for (SizeType i = 0; i < m_materialAssets.GetSize(); ++i)
	{
		m_materialAssets[i]->AwaitInitialization();
	}

	for (SizeType i = 0; i < m_materialAssets.GetSize(); ++i)
	{
		const MaterialAssetHandle& materialAsset = m_materialAssets[i];
		const ecs::EntityHandle matEntity = materialAsset->GetMaterialEntity();
		const mat::MaterialProxyComponent& materialProxy = matEntity.get<mat::MaterialProxyComponent>();

		m_terrainMaterialData.terrainMaterials[i].dataHandle = materialProxy.GetMaterialDataHandle();
		m_terrainMaterialData.terrainMaterials[i].uvScale = compiledData->header.compiledEntries[i].uvScale;
	}
}

} // spt::as
