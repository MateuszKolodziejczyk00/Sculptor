#pragma once

#include "AssetsSystemMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "Utility/ValueOrError.h"
#include "FileSystem/File.h"
#include "ResourcePath.h"


namespace spt::as
{

struct AssetsSystemInitializer
{
	lib::Path contentPath;
};


enum class ECreateError
{
	AlreadyExists
};


enum class EDeleteResult
{
	Success,
	DoesNotExist
};


enum class ELoadError
{
	DoesNotExist
};


using CreateResult = lib::ValueOrError<AssetHandle, ECreateError>;
using LoadResult   = lib::ValueOrError<AssetHandle, ELoadError>;


class ASSETS_SYSTEM_API AssetsSystem
{
public:

	AssetsSystem() = default;
	~AssetsSystem() = default;

	Bool Initialize(const AssetsSystemInitializer& initializer);
	void Shutdown();

	CreateResult CreateAsset(const AssetInitializer& initializer);
	LoadResult   LoadAsset(const ResourcePath& path);
	AssetHandle  LoadAssetChecked(const ResourcePath& path);

	// Current assumption is that it's ok to delete loaded assets - in this case, asset will not be unloaded and it will be possible to save it again
	EDeleteResult DeleteAsset(const ResourcePath& path);

	void SaveAsset(const AssetHandle& asset);

	Bool DoesAssetExist(const ResourcePath& path) const;

	Bool IsLoaded(const ResourcePath& path) const;

	void OnAssetUnloaded(AssetInstance& instance);

	lib::DynamicArray<AssetHandle> GetLoadedAssetsList() const;

private:

	void SaveAssetImpl(AssetHandle asset);

	AssetInstanceData ReadAssetData(const lib::Path& fullPath) const;

	AssetHandle CreateAssetInstance(const AssetInitializer& initializer);

	lib::Path m_contentPath;

	lib::HashMap<ResourcePath, AssetInstance*> m_loadedAssets;

	mutable lib::ReadWriteLock m_assetsSystemLock;
};

} // spt::as