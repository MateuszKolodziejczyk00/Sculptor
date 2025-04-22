#pragma once

#include "AssetsSystemMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "Utility/ValueOrError.h"
#include "FileSystem/File.h"
#include "ResourcePath.h"
#include "DDC.h"


namespace spt::as
{

struct AssetsSystemInitializer
{
	lib::Path contentPath;
	lib::Path ddcPath;
};


enum class ECreateError
{
	AlreadyExists,
	FailedToCreateInstance
};


enum class EDeleteResult
{
	Success,
	DoesNotExist,
	StillReferenced,
};


enum class ELoadError
{
	DoesNotExist,
	FailedToCreateInstance
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

	EDeleteResult DeleteAsset(const ResourcePath& path);

	void SaveAsset(const AssetHandle& asset);

	Bool DoesAssetExist(const ResourcePath& path) const;

	Bool IsLoaded(const ResourcePath& path) const;

	void OnAssetUnloaded(AssetInstance& instance);

	lib::DynamicArray<AssetHandle> GetLoadedAssetsList() const;

	lib::Path GetContentPath() const { return m_contentPath; }

	const DDC& GetDDC() { return m_ddc; }

private:

	void SaveAssetImpl(AssetHandle asset);

	AssetInstanceData ReadAssetData(const lib::Path& fullPath) const;

	AssetHandle CreateAssetInstance(const AssetInitializer& initializer);

	lib::DynamicArray<AssetHandle> GetLoadedAssetsList_Locked() const;

	Bool IsLoaded_Locked(const ResourcePath& path) const;

	lib::Path m_contentPath;

	lib::HashMap<ResourcePath, AssetInstance*> m_loadedAssets;

	DDC m_ddc;

	mutable lib::ReadWriteLock m_assetsSystemLock;
};

} // spt::as