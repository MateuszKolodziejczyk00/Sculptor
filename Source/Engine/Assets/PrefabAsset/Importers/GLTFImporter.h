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

void PREFAB_ASSET_API InitPrefabDefinition(PrefabDefinition& prefabDef, const AssetInstance& asset, const GLTFPrefabDefinition& gltfDef);


struct GLTFImportParams
{
	AssetsSystem& assetsSystem;

	lib::Path srcGLTFPath;
	lib::Path dstContentPath;

	// If true, prefab will just reference source gltf, so when GLTF is changed, prefab will reflect those changes
	// If false, prefab will have all hierarchy expanded so it won't reflect changes in source gltf (although, it will still reference source gltf for mesh and material data)
	Bool importPrefabAsGLTF = true;
};

void PREFAB_ASSET_API ImportGLTF(const GLTFImportParams& params);

} // importer

} // spt::as
