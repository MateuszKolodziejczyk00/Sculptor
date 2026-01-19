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
};

void PREFAB_ASSET_API ImportGLTF(const GLTFImportParams& params);

} // importer

} // spt::as
