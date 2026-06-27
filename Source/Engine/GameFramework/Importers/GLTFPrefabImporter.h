#pragma once

#include "GameFrameworkMacros.h"
#include "SculptorCoreTypes.h"
#include "Importers/PrefabImporter.h"


namespace spt::gf
{

class GLTFPrefabImporter : public as::importer::PrefabImporter
{
public:

	virtual void ImportPrefabDefinition(as::AssetsSystem& assetsSystem, const lib::Path& sourcePath, as::PrefabDefinition& prefabDef) override;
};


struct PrefabImportParams
{
	as::AssetsSystem& assetsSystem;

	lib::Path srcGLTFPath;
	lib::Path dstContentPath;

	// If true, prefab will just reference source gltf, so when GLTF is changed, prefab will reflect those changes
	// If false, prefab will have all hierarchy expanded so it won't reflect changes in source gltf (although, it will still reference source gltf for mesh and material data)
	Bool importPrefabAsReference = true;
};

void PREFAB_ASSET_API ImportGLTF(const PrefabImportParams& params);


} // spt::gf
