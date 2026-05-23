#include "TerrainMaterialAsset.h"
#include "AssetsSystem.h"


SPT_DEFINE_LOG_CATEGORY(TerrainMaterialAsset, true);

namespace spt::as
{

struct TerrainMaterialDerivedDataHeader
{
	lib::StaticArray<ResourcePathID, rsc::terrain_material_props::maxMaterialEntries> materialAssetPathIDs;

	TerrainMaterialDerivedDataHeader()
	{
		materialAssetPathIDs.fill(InvalidResourcePathID);
	}

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MaterialAssetPathIDs", materialAssetPathIDs);
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

rsc::TerrainMaterialData TerrainMaterialAsset::GetTerrainMaterialData() const
{
	rsc::TerrainMaterialData terrainMaterialData;

	for (SizeType i = 0; i < m_materialAssets.GetSize(); ++i)
	{
		if (m_materialAssets[i].IsValid())
		{
			const ecs::EntityHandle matEntity = m_materialAssets[i]->GetMaterialEntity();
			const mat::MaterialProxyComponent& materialProxy = matEntity.get<mat::MaterialProxyComponent>();
			terrainMaterialData.terrainMaterials[i] = materialProxy.GetMaterialDataHandle();
		}
	}

	return terrainMaterialData;
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

		header.materialAssetPathIDs[i] = materialAssetPath.GetID();
	}

	CreateDerivedData(*this, header, lib::Span<const Byte>());

	return true;
}

void TerrainMaterialAsset::OnInitialize()
{
	const lib::MTHandle<DDCLoadedData<TerrainMaterialDerivedDataHeader>> compiledData = LoadDerivedData<TerrainMaterialDerivedDataHeader>(*this);
	SPT_CHECK(compiledData.IsValid());

	for (SizeType i = 0; i < compiledData->header.materialAssetPathIDs.size(); ++i)
	{
		const ResourcePathID materialAssetPathID = compiledData->header.materialAssetPathIDs[i];
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
}

} // spt::as
