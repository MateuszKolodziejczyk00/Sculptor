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
	lib::Path materialIDsTex;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("HeightMapTex",    heightMapTex);
		serializer.Serialize("TerrainMaterial", terrainMaterial);
		serializer.Serialize("MaterialIDsTex",  materialIDsTex);
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

	const TerrainAssetDefinition& GetTerrainAssetDefinition() const;

	rsc::TerrainDefinition GetTerrainDefinition() const;
	rsc::TerrainMaterialsMap GetTerrainMaterialsMap() const;

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	lib::SharedPtr<rdr::TextureView> m_heightMap;
	lib::SharedPtr<rdr::TextureView> m_farLODBaseColor;
	lib::SharedPtr<rdr::TextureView> m_farLODProps;
	lib::SharedPtr<rdr::TextureView> m_materialIDs;
	math::Vector2f m_terrainMinBounds;
	math::Vector2f m_terrainMaxBounds;
	TerrainMaterialAssetHandle       m_terrainMaterialAsset;
};
SPT_REGISTER_ASSET_TYPE(TerrainAsset);

} // spt::as
