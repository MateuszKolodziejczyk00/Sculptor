#pragma once

#include "AssetsSystemMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "Utility/ValueOrError.h"
#include "FileSystem/File.h"
#include "ResourcePath.h"
#include "DDC.h"
#include "AssetsDB.h"


namespace spt::as
{

enum class EAssetsSystemFlags
{
	None = 0u,
	// Allows loading only compiled assets from DDC. Results in faster load times, but asset must be pre-compiled. Assets cannot be saved in this mode
	CompiledOnly = BIT(0),

	Default = None
};


struct AssetsSystemInitializer
{
	lib::Path contentPath;
	lib::Path ddcPath;

	EAssetsSystemFlags flags = EAssetsSystemFlags::Default;
};


enum class ECreateError
{
	AlreadyExists,
	FailedToCreateInstance,
	CompilationFailed,
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

	template<typename TAssetType>
	TypedAssetHandle<TAssetType> LoadAssetChecked(const ResourcePath& path)
	{
		AssetHandle asset = LoadAssetChecked(path);
		SPT_CHECK(asset->GetInstanceData().type == CreateAssetType<TAssetType>());
		return static_cast<TAssetType*>(asset.Get());
	}

	AssetHandle LoadAndInitAssetChecked(const ResourcePath& path);

	template<typename TAssetType>
	TypedAssetHandle<TAssetType> LoadAndInitAssetChecked(const ResourcePath& path)
	{
		AssetHandle asset = LoadAndInitAssetChecked(path);
		SPT_CHECK(asset->GetInstanceData().type == CreateAssetType<TAssetType>());
		return static_cast<TAssetType*>(asset.Get());
	}

	EDeleteResult DeleteAsset(const ResourcePath& path);

	void SaveAsset(const AssetHandle& asset);

	Bool DoesAssetExist(const ResourcePath& path) const;

	Bool IsLoaded(const ResourcePath& path) const;

	void OnAssetUnloaded(AssetInstance& instance);

	lib::DynamicArray<AssetHandle> GetLoadedAssetsList() const;

	const lib::Path& GetContentPath() const { return m_contentPath; }

	DDC& GetDDC() { return m_ddc; }

	Bool IsAssetCompiled(const ResourcePath& path) const;

private:

	void SaveAssetImpl(AssetHandle asset);

	AssetInstanceData ReadAssetData(const lib::Path& fullPath) const;

	AssetHandle CreateAssetInstance(const AssetInitializer& initializer);

	void ScheduleAssetInitialization(const AssetHandle& assetInstance);

	lib::DynamicArray<AssetHandle> GetLoadedAssetsList_Locked() const;

	Bool IsLoaded_Locked(const ResourcePath& path) const;

	Bool CompileAssetImpl(const AssetHandle& asset);

	Bool IsCompiledOnlyMode() const { return lib::HasAnyFlag(m_flags, EAssetsSystemFlags::CompiledOnly); }

	lib::Path m_contentPath;

	EAssetsSystemFlags m_flags = EAssetsSystemFlags::Default;

	lib::HashMap<ResourcePath, AssetInstance*> m_loadedAssets;

	DDC m_ddc;

	AssetsDB m_assetsDB;

	mutable lib::ReadWriteLock m_assetsSystemLock;
};

} // spt::as