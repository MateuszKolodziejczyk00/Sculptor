#include "AssetsSystem.h"
#include "ProfilerCore.h"
#include "ResourcePath.h"
#include "SerializationHelper.h"
#include "JobSystem/JobSystem.h"

SPT_DEFINE_LOG_CATEGORY(AssetsSystem, true);


namespace spt::as
{

Bool AssetsSystem::Initialize(const AssetsSystemInitializer& initializer)
{
	SPT_PROFILER_FUNCTION();

	m_contentPath = initializer.contentPath;
	m_flags       = initializer.flags;

	m_ddc.Initialize({ .path = initializer.ddcPath });

	AssetsDBInitInfo dbInitInfo
	{
		.ddc          = m_ddc,

		.assetsDDCKey = DerivedDataKey(idxNone<Uint64>, 0u),
		.pathsDDCKey  = DerivedDataKey(idxNone<Uint64>, 1u)
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

	AssetHandle assetInstance;

	{
		lib::LockGuard lock(m_assetsSystemLock);

		const lib::Path fullPath = m_contentPath / initializer.path.GetPath();

		if (std::filesystem::exists(fullPath))
		{
			return CreateResult(ECreateError::AlreadyExists);
		}

		const ResourcePathID pathID = initializer.path.GetID();

		// It might happen that there used to be a compiled asset with the same path. In that case, remove it's compiled data
		if (IsAssetCompiled(pathID))
		{
			m_ddc.DeleteDerivedData(AssetDerivedDataKey(pathID));
			m_assetsDB.DeleteAssetDescriptor(pathID);
		}

		assetInstance = CreateAssetInstance(AssetInstanceDefinition
											{
												.type   = initializer.type,
												.name   = initializer.path.GetName(),
												.pathID = pathID
											});

		if (!assetInstance.IsValid())
		{
			return CreateResult(ECreateError::FailedToCreateInstance);
		}

		if (initializer.dataInitializer)
		{
			initializer.dataInitializer->InitializeNewAsset(*assetInstance);
		}

		assetInstance->PostCreate();

		SaveAssetImpl(assetInstance);

		m_loadedAssets[pathID] = assetInstance.Get();

		SPT_CHECK(!IsAssetCompiled(assetInstance->GetResourcePathID()));
		if (IsCompiledOnlyMode())
		{
			return CreateResult(ECreateError::CompilationFailed);
		}
	}

	ScheduleAssetInitialization(assetInstance);

	return CreateResult(std::move(assetInstance));
}

LoadResult<> AssetsSystem::LoadAsset(ResourcePathID pathID)
{
	SPT_PROFILER_FUNCTION();

	if (pathID == InvalidResourcePathID)
	{
		return LoadResult(ELoadError::DoesNotExist);
	}

	AssetHandle assetInstance;

	{
		lib::LockGuard lock(m_assetsSystemLock);

		{
			const auto it = m_loadedAssets.find(pathID);
			if (it != m_loadedAssets.end())
			{
				return LoadResult(AssetHandle(it->second));
			}
		}
	
		AssetInstanceData assetData;
		if (IsCompiledOnlyMode())
		{
			const std::optional<AssetDescriptor> descriptor = m_assetsDB.GetAssetDescriptor(pathID);
			if (!descriptor)
			{
				return LoadResult(ELoadError::DoesNotExist);
			}
	
			assetData.type = AssetFactory::GetInstance().GetAssetTypeByKey(descriptor->assetTypeKey);
		}
		else
		{
			const ResourcePath path = ResolvePath(pathID);
			SPT_CHECK(path.IsValid());

			const lib::Path fullPath = m_contentPath / path.GetPath();
	
			if (!std::filesystem::exists(fullPath))
			{
				return LoadResult(ELoadError::DoesNotExist);
			}
	
			assetData = ReadAssetData(fullPath);
		}
	
		assetInstance = CreateAssetInstance(AssetInstanceDefinition
											{
												.type   = assetData.type,
												.name   = CreateAssetName(pathID),
												.pathID = pathID,
											});
	
		if (!assetInstance.IsValid())
		{
			return ELoadError::FailedToCreateInstance;
		}
	
		m_loadedAssets[pathID] = assetInstance.Get();
	
		assetInstance->AssignData(std::move(assetData));
	}

	SPT_CHECK(assetInstance.IsValid());

	ScheduleAssetInitialization(assetInstance);

	return LoadResult(AssetHandle(std::move(assetInstance)));
}

AssetHandle AssetsSystem::LoadAssetChecked(ResourcePathID pathID)
{
	SPT_PROFILER_FUNCTION();

	const LoadResult loadResult = LoadAsset(pathID);
	SPT_CHECK(loadResult.HasValue());

	return AssetHandle(loadResult.GetValue());
}

AssetHandle AssetsSystem::LoadAndInitAssetChecked(ResourcePathID pathID)
{
	AssetHandle asset = LoadAssetChecked(pathID);
	asset->AwaitInitialization();
	return asset;
}

AssetHandle AssetsSystem::GetLoadedAsset(ResourcePathID pathID) const
{
	AssetHandle result;

	{
		lib::LockGuard lock(m_assetsSystemLock);

		const auto it = m_loadedAssets.find(pathID);
		if (it != m_loadedAssets.end())
		{
			result = it->second;
		}
	}

	return result;
}

Bool AssetsSystem::CompileAssetIfDeprecated(const ResourcePath& path)
{
	if (!DoesAssetExist(path))
	{
		return false;
	}

	if (!IsAssetCompiled(path.GetID()) || !IsAssetUpToDate(path))
	{
		SPT_CHECK(DoesAssetExist(path));

		LoadAndInitAssetChecked(path.GetID());
	}

	return true;
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

		if (IsLoaded_Locked(path.GetID()))
		{
			return EDeleteResult::StillReferenced;
		}

		assetData = ReadAssetData(fullPath);

		std::filesystem::remove(fullPath);
	}

	AssetFactory& factory = AssetFactory::GetInstance();
	factory.DeleteAsset(assetData.type, *this, path, assetData);
	m_ddc.DeleteDerivedData(AssetDerivedDataKey(path.GetID()));

	SPT_CHECK(!IsAssetCompiled(path.GetID())); // Derived data left?

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

Bool AssetsSystem::IsLoaded(ResourcePathID pathID) const
{
	lib::LockGuard lock(m_assetsSystemLock);

	return IsLoaded_Locked(pathID);
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

		for (const AssetHandle& asset : loadedAssets)
		{
			asset->ClearPermanent();
		}

		loadedAssets.clear();

		{
			lib::LockGuard lock(m_assetsSystemLock);
			loadedAssets = GetLoadedAssetsList_Locked();
		}

		Uint32 notUnloadedAssetsNum = 0u;
		for (const AssetHandle& asset : loadedAssets)
		{
			SPT_LOG_ERROR(AssetsSystem, "Asset was not unloaded! Name: {}", asset->GetName().ToString());
			++notUnloadedAssetsNum;
		}


		if (notUnloadedAssetsNum > 0u)
		{
			SPT_CHECK_NO_ENTRY_MSG("Not all assets were unloaded before shutdown!");
		}
	}
}

void AssetsSystem::RemoveAssetCompiledData(ResourcePathID pathID)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsCompiledOnlyMode());

	m_assetsDB.DeleteAssetDescriptor(pathID);
	m_ddc.DeleteDerivedData(AssetDerivedDataKey(pathID));
}

void AssetsSystem::RemoveAssetsCompiledDataByType(AssetType type)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsCompiledOnlyMode());

	const lib::DynamicArray<AssetMetaData> assets = m_assetsDB.GetAllAssetsDescriptors();
	for (const AssetMetaData& assetMeta : assets)
	{
		if (assetMeta.descriptor.assetTypeKey == type.GetKey())
		{
			RemoveAssetCompiledData(assetMeta.pathID);
		}
	}
}

void AssetsSystem::RemoveAllAssetsCompiledData()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsCompiledOnlyMode());

	const lib::DynamicArray<AssetMetaData> assets = m_assetsDB.GetAllAssetsDescriptors();
	for (const AssetMetaData& assetMeta : assets)
	{
		RemoveAssetCompiledData(assetMeta.pathID);
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

	m_loadedAssets.erase(instance.GetResourcePathID());
}

Bool AssetsSystem::IsAssetCompiled(const ResourcePathID pathID) const
{
	return m_assetsDB.ContainsAsset(pathID);
}

Bool AssetsSystem::IsAssetUpToDate(const ResourcePath& path) const
{
	const AssetDerivedDataKey mainKey(path.GetID());

	const lib::Path ddcPath = m_ddc.GetDerivedDataPath(mainKey.ddcKey);

	if (!std::filesystem::exists(ddcPath))
	{
		return false;
	}

	const auto ddcLastWriteTime = std::filesystem::last_write_time(ddcPath);
	const auto assetLastWriteTime = std::filesystem::last_write_time(GetContentPath() / path.GetPath());

	return ddcLastWriteTime >= assetLastWriteTime;
}

ResourcePath AssetsSystem::ResolvePath(const ResourcePathID pathID) const
{
	SPT_CHECK(!IsCompiledOnlyMode());

	ResourcePath path = ResourcePath::GetCachedPath(pathID);
	if (!path.IsValid())
	{
		path = m_assetsDB.GetPath(pathID);
	}

	return path;
}

void AssetsSystem::SaveAssetImpl(AssetHandle asset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(asset.IsValid());
	SPT_CHECK(!IsCompiledOnlyMode());

	asset->PreSave();

	const lib::String data = srl::SerializationHelper::SerializeStruct(asset->GetInstanceData());

	const ResourcePath resolvedPath = ResolvePath(asset->GetResourcePathID());
	SPT_CHECK(resolvedPath.IsValid());
	const lib::Path fullPath = m_contentPath / resolvedPath.GetPath();
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

AssetHandle AssetsSystem::CreateAssetInstance(const AssetInstanceDefinition& definition)
{
	AssetFactory& factory = AssetFactory::GetInstance();

	return factory.CreateAsset(*this, definition);
}

void AssetsSystem::ScheduleAssetInitialization(const AssetHandle& assetInstance)
{
	Bool isCompiled = false;

	if (IsCompiledOnlyMode())
	{
		SPT_CHECK(IsAssetCompiled(assetInstance->GetResourcePathID()));
		isCompiled = true;
	}
	else
	{
		const ResourcePath path = ResolvePath(assetInstance->GetResourcePathID());
		SPT_CHECK(path.IsValid());
		isCompiled = IsAssetCompiled(assetInstance->GetResourcePathID()) && IsAssetUpToDate(path);
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

Bool AssetsSystem::IsLoaded_Locked(ResourcePathID pathID) const
{
	return m_loadedAssets.contains(pathID);
}

Bool AssetsSystem::CompileAssetImpl(const AssetHandle& asset)
{
	SPT_PROFILER_FUNCTION();

	m_compilationInputCache.OnCompilationStarted();
	SPT_LOG_INFO(AssetsSystem, "Compiling asset: {}", ResolvePath(asset->GetResourcePathID()).GetPath().string());

	const Bool compilationResult = asset->Compile();

	if (compilationResult)
	{
		m_assetsDB.SaveAssetDescriptor(ResolvePath(asset->GetResourcePathID()), AssetDescriptor{ .assetTypeKey = asset->GetTypeKey() });
		SPT_LOG_INFO(AssetsSystem, "Successfully compiled asset: {}", ResolvePath(asset->GetResourcePathID()).GetPath().string());
	}
	else
	{
		SPT_LOG_ERROR(AssetsSystem, "Failed to compile asset: {}", ResolvePath(asset->GetResourcePathID()).GetPath().string());
	}

	m_compilationInputCache.OnCompilationFinished();

	return compilationResult;
}

lib::HashedString AssetsSystem::CreateAssetName(ResourcePathID pathID) const
{
	if (IsCompiledOnlyMode())
	{
		return lib::HashedString(lib::StringUtils::ToHexString(reinterpret_cast<Byte*>(&pathID), sizeof(ResourcePathID)));
	}
	else
	{
		const ResourcePath resolvedPath = ResolvePath(pathID);
		SPT_CHECK(resolvedPath.IsValid());
		return resolvedPath.GetName();
	}
}

} // spt::as
