#pragma once

#include "MaterialAsset.h"
#include "Terrain/TerrainDefinition.h"


namespace spt::as
{

struct TerrainMaterialDefinition
{
	ResourcePath materialAsset;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MaterialAsset", materialAsset);
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

	const MaterialAssetHandle& GetMaterialAsset() const { return m_materialAsset; }

	rsc::TerrainMaterialData GetTerrainMaterialData() const;

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	MaterialAssetHandle m_materialAsset;
};
SPT_REGISTER_ASSET_TYPE(TerrainMaterialAsset);

} // spt::as
