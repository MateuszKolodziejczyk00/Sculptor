#pragma once

#include "AssetsSystemMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "Utility/ValueOrError.h"
#include "FileSystem/File.h"
#include "ResourcePath.h"
#include "DDC.h"
#include "CompilationInputCache.h"
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

template<typename TAssetType = AssetInstance>
using LoadResult = lib::ValueOrError<TypedAssetHandle<TAssetType>, ELoadError>;



class ASSETS_SYSTEM_API AssetsSystem
{
public:

	AssetsSystem() = default;
	~AssetsSystem() = default;

	Bool Initialize(const AssetsSystemInitializer& initializer);
	void Shutdown();

	CreateResult CreateAsset(const AssetInitializer& initializer);

	LoadResult<> LoadAsset(ResourcePathID pathID);
	AssetHandle  LoadAssetChecked(ResourcePathID pathID);

	template<typename TAssetType>
	LoadResult<TAssetType> LoadAsset(ResourcePathID pathID)
	{
		LoadResult<> loadRes = LoadAsset(pathID);
		SPT_CHECK(loadRes.HasValue() ? loadRes.GetValue()->GetInstanceData().type == CreateAssetType<TAssetType>() : true);
		return loadRes.HasValue() 
			  ? LoadResult<TAssetType>(reinterpret_cast<TAssetType*>(loadRes.GetValue().Get()))
			  : LoadResult<TAssetType>(loadRes.GetError());
	}

	template<typename TAssetType>
	TypedAssetHandle<TAssetType> LoadAssetChecked(ResourcePathID pathID)
	{
		AssetHandle asset = LoadAssetChecked(pathID);
		SPT_CHECK(asset->GetInstanceData().type == CreateAssetType<TAssetType>());
		return static_cast<TAssetType*>(asset.Get());
	}

	AssetHandle LoadAndInitAssetChecked(ResourcePathID pathID);

	template<typename TAssetType>
	TypedAssetHandle<TAssetType> LoadAndInitAssetChecked(ResourcePathID pathID)
	{
		AssetHandle asset = LoadAndInitAssetChecked(pathID);
		SPT_CHECK(asset->GetInstanceData().type == CreateAssetType<TAssetType>());
		return static_cast<TAssetType*>(asset.Get());
	}

	AssetHandle GetLoadedAsset(ResourcePathID pathID) const;

	template<typename TAssetType>
	TypedAssetHandle<TAssetType> GetLoadedAsset(ResourcePathID pathID)
	{
		const AssetHandle asset = GetLoadedAsset(pathID);
		SPT_CHECK(!asset.IsValid() || asset->GetInstanceData().type == CreateAssetType<TAssetType>());
		return static_cast<TAssetType*>(asset.Get());
	}

	Bool CompileAssetIfDeprecated(const ResourcePath& path);

	EDeleteResult DeleteAsset(const ResourcePath& path);

	void SaveAsset(const AssetHandle& asset);

	Bool DoesAssetExist(const ResourcePath& path) const;

	Bool IsLoaded(ResourcePathID pathID) const;

	void OnAssetUnloaded(AssetInstance& instance);

	void UnloadPermanentAssets();

	lib::DynamicArray<AssetHandle> GetLoadedAssetsList() const;

	const lib::Path& GetContentPath() const { return m_contentPath; }

	DDC& GetDDC() { return m_ddc; }

	Bool IsAssetCompiled(const ResourcePathID pathID) const;
	Bool IsAssetUpToDate(const ResourcePath& path) const;

	ResourcePath ResolvePath(const ResourcePathID pathID) const;

	template<typename TType, typename TLoader>
	const TType& GetCompilationInputData(const lib::HashedString& key, TLoader&& loader);

private:

	void SaveAssetImpl(AssetHandle asset);

	AssetInstanceData ReadAssetData(const lib::Path& fullPath) const;

	AssetHandle CreateAssetInstance(const AssetInstanceDefinition& initializer);

	void ScheduleAssetInitialization(const AssetHandle& assetInstance);

	lib::DynamicArray<AssetHandle> GetLoadedAssetsList_Locked() const;

	Bool IsLoaded_Locked(ResourcePathID pathID) const;

	Bool CompileAssetImpl(const AssetHandle& asset);

	lib::HashedString CreateAssetName(ResourcePathID pathID) const;

	Bool IsCompiledOnlyMode() const { return lib::HasAnyFlag(m_flags, EAssetsSystemFlags::CompiledOnly); }

	lib::Path m_contentPath;

	EAssetsSystemFlags m_flags = EAssetsSystemFlags::Default;

	lib::HashMap<ResourcePathID, AssetInstance*> m_loadedAssets;

	DDC m_ddc;

	AssetsDB m_assetsDB;

	mutable lib::ReadWriteLock m_assetsSystemLock;

	CompilationInputCache m_compilationInputCache;
};


template<typename TType, typename TLoader>
const TType& AssetsSystem::GetCompilationInputData(const lib::HashedString& key, TLoader&& loader)
{
	return m_compilationInputCache.GetOrLoadData<TType>(key, std::forward<TLoader>(loader));
}

} // spt::as
