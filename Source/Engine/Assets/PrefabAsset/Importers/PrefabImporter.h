#pragma once

#include "PrefabAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "PrefabAsset.h"


namespace tinygltf
{
struct Material;
struct Mesh;
} // tinygltf


namespace spt::as
{

struct PrefabDefinition;
struct GLTFPrefabDefinition;
class AssetInstance;
class AssetsSystem;

namespace importer
{

class PREFAB_ASSET_API PrefabImporter
{
public:

	virtual ~PrefabImporter() = default;

	virtual void ImportPrefabDefinition(as::AssetsSystem& assetsSystem, const lib::Path& sourcePath, as::PrefabDefinition& prefabDef) = 0;
};

void PREFAB_ASSET_API RegisterImporter(lib::String importerName, lib::UniquePtr<PrefabImporter> importer);

void PREFAB_ASSET_API ImportPrefabDefinition(as::AssetsSystem& assetsSystem, const lib::Path& sourcePath, PrefabDefinition& prefabDef);


template<typename TImporter, lib::Literal extension>
struct PREFAB_ASSET_API ImporterRegistration
{
	ImporterRegistration()
	{
		RegisterImporter(extension.Get(), lib::MakeUnique<TImporter>());
	}
};

} // importer

} // spt::as

#define SPT_REGISTER_PREFAB_IMPORTER(importerType, extension) \
	static spt::as::importer::ImporterRegistration<importerType, extension> importerType##Registration;
