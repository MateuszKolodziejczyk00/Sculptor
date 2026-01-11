#include "AssetsSystem.h"
#include "ProfilerCore.h"
#include "SerializationHelper.h"
#include "JobSystem/JobSystem.h"

SPT_DEFINE_LOG_CATEGORY(AssetsSystem, true);


namespace spt::as
{

Bool AssetsSystem::Initialize(const AssetsSystemInitializer& initializer)
{
	m_contentPath = initializer.contentPath;
	m_flags       = initializer.flags;

	m_ddc.Initialize({ .path = initializer.ddcPath });

	AssetsDBInitInfo dbInitInfo
	{
		.ddc = m_ddc,
		.ddcKey = DerivedDataKey(idxNone<Uint64>, 0u)
	};

	m_assetsDB.Initalize(dbInitInfo);

	return true;
}

void AssetsSystem::Shutdown()
{
	UnloadPermanentAssets();

	{
		lib::LockGuard lock(m_assetsSystemLock);
		m_assetsDB.Shutdown();
	}
}

CreateResult AssetsSystem::CreateAsset(const AssetInitializer& initializer)
{
	SPT_PROFILER_FUNCTION();

	lib::LockGuard lock(m_assetsSystemLock);

	const lib::Path fullPath = m_contentPath / initializer.path.GetPath();

	if (std::filesystem::exists(fullPath))
	{
		return CreateResult(ECreateError::AlreadyExists);
	}

	const AssetHandle assetInstance = CreateAssetInstance(initializer);

	if (!assetInstance.IsValid())
	{
		return CreateResult(ECreateError::FailedToCreateInstance);
	}

	assetInstance->PostCreate();

	SaveAssetImpl(assetInstance);

	m_loadedAssets[initializer.path] = assetInstance.Get();

	SPT_CHECK(!IsAssetCompiled(assetInstance->GetRelativePath()));
	if (!CompileAssetImpl(assetInstance) && IsCompiledOnlyMode())
	{
		return CreateResult(ECreateError::CompilationFailed);
	}

	SPT_CHECK(IsAssetCompiled(assetInstance->GetRelativePath()));

	assetInstance->Initialize();

	return CreateResult(std::move(assetInstance));
}

LoadResult AssetsSystem::LoadAsset(const ResourcePath& path)
{
	SPT_PROFILER_FUNCTION();

	AssetHandle assetInstance;

	{
		lib::LockGuard lock(m_assetsSystemLock);

		{
			const auto it = m_loadedAssets.find(path);
			if (it != m_loadedAssets.end())
			{
				return LoadResult(AssetHandle(it->second));
			}
		}
	
		AssetInstanceData assetData;
		if (IsCompiledOnlyMode())
		{
			const std::optional<AssetDescriptor> descriptor = m_assetsDB.GetAssetDescriptor(path);
			if (!descriptor)
			{
				return LoadResult(ELoadError::DoesNotExist);
			}
	
			assetData.type = AssetFactory::GetInstance().GetAssetTypeByKey(descriptor->assetTypeKey);
		}
		else
		{
			const lib::Path fullPath = m_contentPath / path.GetPath();
	
			if (!std::filesystem::exists(fullPath))
			{
				return LoadResult(ELoadError::DoesNotExist);
			}
	
			assetData = ReadAssetData(fullPath);
		}
	
		assetInstance = CreateAssetInstance(AssetInitializer
											{
												.type = assetData.type,
												.path = path,
											});
	
		if (!assetInstance.IsValid())
		{
			return ELoadError::FailedToCreateInstance;
		}
	
		m_loadedAssets[path] = assetInstance.Get();
	
		assetInstance->AssignData(std::move(assetData));
	}

	SPT_CHECK(assetInstance.IsValid());

	ScheduleAssetInitialization(assetInstance);

	return LoadResult(AssetHandle(std::move(assetInstance)));
}

AssetHandle AssetsSystem::LoadAssetChecked(const ResourcePath& path)
{
	SPT_PROFILER_FUNCTION();

	const LoadResult loadResult = LoadAsset(path);
	SPT_CHECK(loadResult.HasValue());

	return AssetHandle(loadResult.GetValue());
}

AssetHandle AssetsSystem::LoadAndInitAssetChecked(const ResourcePath& path)
{
	AssetHandle asset = LoadAssetChecked(path);
	asset->AwaitInitialization();
	return asset;
}

EDeleteResult AssetsSystem::DeleteAsset(const ResourcePath& path)
{
	SPT_PROFILER_FUNCTION();

	const lib::Path fullPath = m_contentPath / path.GetPath();

	if (!std::filesystem::exists(fullPath))
	{
		return EDeleteResult::DoesNotExist;
	}

	AssetInstanceData assetData;

	{
		lib::LockGuard lock(m_assetsSystemLock);

		if (IsLoaded_Locked(path))
		{
			return EDeleteResult::StillReferenced;
		}

		assetData = ReadAssetData(fullPath);

		std::filesystem::remove(fullPath);
	}

	AssetFactory& factory = AssetFactory::GetInstance();
	factory.DeleteAsset(assetData.type, *this, path, assetData);
	m_ddc.DeleteDerivedData(AssetDerivedDataKey(path));

	SPT_CHECK(!IsAssetCompiled(path)); // Derived data left?

	return EDeleteResult::Success;
}

void AssetsSystem::SaveAsset(const AssetHandle& asset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsCompiledOnlyMode());

	SaveAssetImpl(asset);
}

Bool AssetsSystem::DoesAssetExist(const ResourcePath& path) const
{
	return std::filesystem::exists(m_contentPath / path.GetPath());
}

Bool AssetsSystem::IsLoaded(const ResourcePath& path) const
{
	lib::LockGuard lock(m_assetsSystemLock);

	return IsLoaded_Locked(path);
}

void AssetsSystem::UnloadPermanentAssets()
{
	SPT_PROFILER_FUNCTION();

	if (!m_loadedAssets.empty())
	{
		lib::DynamicArray<AssetHandle> loadedAssets;

		{
			lib::LockGuard lock(m_assetsSystemLock);
			loadedAssets = GetLoadedAssetsList_Locked();
		}

		Uint32 notUnloadedAssetsNum = 0u;

		for (const AssetHandle& asset : loadedAssets)
		{
			if (!asset->ClearPermanent())
			{
				SPT_LOG_ERROR(AssetsSystem, "Asset was not unloaded! Name: {}", asset->GetName().ToString());
				++notUnloadedAssetsNum;
			}
		}

		if (notUnloadedAssetsNum > 0u)
		{
			SPT_CHECK_NO_ENTRY_MSG("Not all assets were unloaded before shutdown!");
		}
	}
}

lib::DynamicArray<AssetHandle> AssetsSystem::GetLoadedAssetsList() const
{
	SPT_PROFILER_FUNCTION();

	lib::LockGuard lock(m_assetsSystemLock);

	return GetLoadedAssetsList_Locked();
}

void AssetsSystem::OnAssetUnloaded(AssetInstance& instance)
{
	instance.PreUnload();

	lib::LockGuard lock(m_assetsSystemLock);

	SPT_CHECK(instance.GetRefCount() == 0u);

	m_loadedAssets.erase(instance.GetRelativePath());
}

Bool AssetsSystem::IsAssetCompiled(const ResourcePath& path) const
{
	const AssetDerivedDataKey mainKey(path);

	const lib::Path ddcPath = m_ddc.GetDerivedDataPath(mainKey.ddcKey);

	if (!std::filesystem::exists(ddcPath))
	{
		return false;
	}

	const auto ddcLastWriteTime = std::filesystem::last_write_time(ddcPath);
	const auto assetLastWriteTime = std::filesystem::last_write_time(GetContentPath() / path.GetPath());

	return ddcLastWriteTime >= assetLastWriteTime;
}

void AssetsSystem::SaveAssetImpl(AssetHandle asset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(asset.IsValid());
	SPT_CHECK(!IsCompiledOnlyMode());

	asset->PreSave();

	const lib::String data = srl::SerializationHelper::SerializeStruct(asset->GetInstanceData());

	const lib::Path fullPath = m_contentPath / asset->GetRelativePath();
	lib::File::SaveDocument(fullPath, data, lib::EFileOpenFlags::ForceCreate);

	asset->PostSave();
}

AssetInstanceData AssetsSystem::ReadAssetData(const lib::Path& fullPath) const
{
	const lib::String fileContent = lib::File::ReadDocument(fullPath);

	AssetInstanceData assetData;

	if (!fileContent.empty())
	{
		srl::SerializationHelper::DeserializeStruct(assetData, fileContent);
	}
	else
	{
		SPT_LOG_ERROR(AssetsSystem, "Failed to read asset data from file: {}", fullPath.string());
	}

	return assetData;
}

AssetHandle AssetsSystem::CreateAssetInstance(const AssetInitializer& initializer)
{
	AssetFactory& factory = AssetFactory::GetInstance();

	AssetHandle newAsset = factory.CreateAsset(*this, initializer);

	if (newAsset.IsValid() && initializer.dataInitializer)
	{
		initializer.dataInitializer->InitializeNewAsset(*newAsset);
	}

	return newAsset;
}

void AssetsSystem::ScheduleAssetInitialization(const AssetHandle& assetInstance)
{
	Bool isCompiled = false;

	if (IsCompiledOnlyMode())
	{
		SPT_CHECK(IsAssetCompiled(assetInstance->GetRelativePath()));
		isCompiled = true;
	}
	else
	{
		isCompiled = IsAssetCompiled(assetInstance->GetRelativePath());
	}

	js::Job initJob = js::Launch(SPT_GENERIC_JOB_NAME,
								 [this, assetInstance, isCompiled]()
								 {
									 if (!isCompiled)
									 {
										 CompileAssetImpl(assetInstance);
									 }
					
									 assetInstance->Initialize();
								 });

	assetInstance->AssignInitializationJob(std::move(initJob));
}

lib::DynamicArray<AssetHandle> AssetsSystem::GetLoadedAssetsList_Locked() const
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<AssetHandle> result;
	result.reserve(m_loadedAssets.size());

	for (const auto& [path, asset] : m_loadedAssets)
	{
		result.emplace_back(asset);
	}

	return result;
}

Bool AssetsSystem::IsLoaded_Locked(const ResourcePath& path) const
{
	return m_loadedAssets.contains(path);
}

Bool AssetsSystem::CompileAssetImpl(const AssetHandle& asset)
{
	SPT_PROFILER_FUNCTION();

	const Bool compilationResult = asset->Compile();

	if (compilationResult)
	{
		m_assetsDB.SaveAssetDescriptor(asset->GetRelativePath(), AssetDescriptor{ .assetTypeKey = asset->GetTypeKey() });
	}
	else
	{
		SPT_LOG_ERROR(AssetsSystem, "Failed to compile asset: {}", asset->GetRelativePath().string());
	}

	return compilationResult;
}

} // spt::as
