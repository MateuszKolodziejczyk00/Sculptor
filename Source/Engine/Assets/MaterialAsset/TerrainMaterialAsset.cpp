#include "TerrainMaterialAsset.h"
#include "AssetsSystem.h"


SPT_DEFINE_LOG_CATEGORY(TerrainMaterialAsset, true);

namespace spt::as
{

struct TerrainMaterialDerivedDataHeader
{
	ResourcePathID materialAssetPathID = InvalidResourcePathID;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MaterialAssetPathID", materialAssetPathID);
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

	if (m_materialAsset.IsValid())
	{
		const ecs::EntityHandle matEntity = m_materialAsset->GetMaterialEntity();
		const mat::MaterialProxyComponent& materialProxy = matEntity.get<mat::MaterialProxyComponent>();
		terrainMaterialData.terrainMaterial = materialProxy.GetMaterialDataHandle();
	}

	return terrainMaterialData;
}

Bool TerrainMaterialAsset::Compile()
{
	const TerrainMaterialDefinition& definition = GetBlackboard().Get<TerrainMaterialDefinition>();
	if (!definition.materialAsset.IsValid())
	{
		SPT_LOG_ERROR(TerrainMaterialAsset, "Failed to compile TerrainMaterialAsset '{}' - referenced MaterialAsset path is invalid", GetName().ToString());
		return false;
	}

	const ResourcePath materialAssetPath = ResolveAssetRelativePath(definition.materialAsset.GetPath());
	if (!GetOwningSystem().CompileAssetIfDeprecated(materialAssetPath))
	{
		SPT_LOG_ERROR(TerrainMaterialAsset, "Failed to compile TerrainMaterialAsset '{}' - referenced MaterialAsset '{}' could not be compiled", GetName().ToString(), materialAssetPath.GetPath().generic_string());
		return false;
	}

	TerrainMaterialDerivedDataHeader header;
	header.materialAssetPathID = materialAssetPath.GetID();

	CreateDerivedData(*this, header, lib::Span<const Byte>());

	return true;
}

void TerrainMaterialAsset::OnInitialize()
{
	const lib::MTHandle<DDCLoadedData<TerrainMaterialDerivedDataHeader>> compiledData = LoadDerivedData<TerrainMaterialDerivedDataHeader>(*this);
	SPT_CHECK(compiledData.IsValid());
	SPT_CHECK(compiledData->header.materialAssetPathID != InvalidResourcePathID);

	const LoadResult<MaterialAsset> loadRes = GetOwningSystem().LoadAsset<MaterialAsset>(compiledData->header.materialAssetPathID);
	if (!loadRes)
	{
		SPT_LOG_ERROR(TerrainMaterialAsset, "Failed to load referenced MaterialAsset for TerrainMaterialAsset '{}'. Reason: {}", GetName().ToString(), AssetLoadErrorToString(loadRes.GetError()));
		return;
	}

	m_materialAsset = loadRes.GetValue();
	m_materialAsset->AwaitInitialization();
}

} // spt::as
