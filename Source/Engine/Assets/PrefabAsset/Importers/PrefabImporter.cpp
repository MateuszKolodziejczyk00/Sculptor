#include "PrefabImporter.h"
#include "AssetsSystem.h"
#include "Eigen/src/Geometry/Transform.h"
#include "Eigen/src/Geometry/Translation.h"
#include "Loaders/GLTF.h"
#include "MaterialInstance/PBRMaterialInstance.h"
#include "MeshAsset.h"
#include "PrefabAsset.h"
#include "ProfilerCore.h"
#include "Engine.h"


namespace spt::as::importer
{

SPT_DEFINE_LOG_CATEGORY(ImportersRegistry, true);

struct ImportersRegistry
{
	lib::HashMap<lib::String, lib::UniquePtr<PrefabImporter>> importers;
};

static ImportersRegistry importersRegistry;


void RegisterImporter(lib::String importerName, lib::UniquePtr<PrefabImporter> importer)
{
	ImportersRegistry& registry = importersRegistry;
	SPT_CHECK(!registry.importers.contains(importerName));
	registry.importers.emplace(std::move(importerName), std::move(importer));
}


void ImportPrefabDefinition(as::AssetsSystem& assetsSystem, const lib::Path& sourcePath, PrefabDefinition& prefabDef)
{
	SPT_PROFILER_FUNCTION();

	ImportersRegistry& registry = importersRegistry;

	const lib::String importerName = sourcePath.extension().generic_string();

	const auto importerIt = registry.importers.find(importerName);
	if (importerIt == registry.importers.end())
	{
		SPT_LOG_ERROR(ImportersRegistry, "Importer not found for extension: {}", importerName);
		return;
	}

	importerIt->second->ImportPrefabDefinition(assetsSystem, sourcePath, OUT prefabDef);
}

} // spt::as::importer
