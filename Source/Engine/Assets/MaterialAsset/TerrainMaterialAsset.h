#pragma once

#include "MaterialAsset.h"
#include "Terrain/TerrainDefinition.h"


namespace spt::as
{

struct TerrainMaterialEntry
{
	ResourcePath materialAsset;
	Real32       uvTileMeters = 1.f;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MaterialAsset", materialAsset);
		serializer.Serialize("UVTileMeters",  uvTileMeters);
	}
};


struct TerrainMaterialDefinition
{
	lib::InlineDynamicArray<TerrainMaterialEntry, rsc::terrain_material_props::maxMaterialEntries> materialEntries;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MaterialEntries", materialEntries);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(TerrainMaterialDefinition);


class MATERIAL_ASSET_API TerrainMaterialInitializer : public AssetDataInitializer
{
public:

	explicit TerrainMaterialInitializer(TerrainMaterialDefinition definition)
		: m_definition(std::move(definition))
	{ }

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	TerrainMaterialDefinition m_definition;
};


class MATERIAL_ASSET_API TerrainMaterialAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(TerrainMaterialAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	const rsc::TerrainMaterialData& GetTerrainMaterialData() const;

	lib::Span<const MaterialAssetHandle> GetMaterialAssets() const { return m_materialAssets; }

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	lib::InlineDynamicArray<MaterialAssetHandle, rsc::terrain_material_props::maxMaterialEntries> m_materialAssets;

	rsc::TerrainMaterialData m_terrainMaterialData;
};
SPT_REGISTER_ASSET_TYPE(TerrainMaterialAsset);

} // spt::as
