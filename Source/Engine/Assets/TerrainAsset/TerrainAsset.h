#pragma once

#include "TerrainAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "DDC.h"
#include "Terrain/TerrainDefinition.h"
#include "TerrainMaterialAsset.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{

struct TerrainAssetDefinition
{
	lib::Path heightMapTex;
	lib::Path terrainMaterial;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("HeightMapTex",    heightMapTex);
		serializer.Serialize("TerrainMaterial", terrainMaterial);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(TerrainAssetDefinition);


class TERRAIN_ASSET_API TerrainAssetInitializer : public AssetDataInitializer
{
public:

	explicit TerrainAssetInitializer(TerrainAssetDefinition definition)
		: m_definition(std::move(definition))
	{ }

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	TerrainAssetDefinition m_definition;
};


class TERRAIN_ASSET_API TerrainAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(TerrainAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	const lib::SharedPtr<rdr::TextureView>& GetHeightMap() const { return m_heightMap; }
	const TerrainMaterialAssetHandle& GetTerrainMaterialAsset() const { return m_terrainMaterialAsset; }

	rsc::TerrainDefinition GetTerrainDefinition() const;

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	lib::SharedPtr<rdr::TextureView> m_heightMap;
	lib::SharedPtr<rdr::TextureView> m_farLODBaseColor;
	lib::SharedPtr<rdr::TextureView> m_farLODProps;
	TerrainMaterialAssetHandle       m_terrainMaterialAsset;
};
SPT_REGISTER_ASSET_TYPE(TerrainAsset);

} // spt::as
