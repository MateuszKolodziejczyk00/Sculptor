#pragma once

#include "SceneAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "PrefabAsset.h"
#include "TerrainAsset.h"
#include "CloudscapeAsset.h"


namespace spt::rsc
{
struct RenderSceneSettings;
} // spt::rsc


namespace spt::as
{

struct SceneDefinition
{
	lib::Path prefab;
	lib::Path terrain;
	lib::Path cloudscape;

	// Partial RenderSceneSettings, applied on top of the settings the scene is loaded with
	srl::JSON sceneSettings;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Prefab",        prefab);
		serializer.Serialize("Terrain",       terrain);
		serializer.Serialize("Cloudscape",    cloudscape);
		serializer.Serialize("SceneSettings", sceneSettings);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(SceneDefinition);


class SCENE_ASSET_API SceneAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(SceneAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	const PrefabAssetHandle&     GetPrefab() const     { return m_prefab; }
	const TerrainAssetHandle&    GetTerrain() const    { return m_terrain; }
	const CloudscapeAssetHandle& GetCloudscape() const { return m_cloudscape; }

	void ApplySceneSettings(rsc::RenderSceneSettings& settings) const;

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	PrefabAssetHandle     m_prefab;
	TerrainAssetHandle    m_terrain;
	CloudscapeAssetHandle m_cloudscape;

	srl::JSON m_sceneSettings;
};
SPT_REGISTER_ASSET_TYPE(SceneAsset);

} // spt::as
