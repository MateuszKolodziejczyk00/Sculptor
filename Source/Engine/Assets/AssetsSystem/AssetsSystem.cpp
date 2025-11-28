#include "AssetsSystem.h"
#include "SerializationHelper.h"

SPT_DEFINE_LOG_CATEGORY(AssetsSystem, true);


namespace spt::as
{

Bool AssetsSystem::Initialize(const AssetsSystemInitializer& initializer)
{
	m_contentPath = initializer.contentPath;

	m_ddc.Initialize({ .path = initializer.ddcPath });

	return true;
}

void AssetsSystem::Shutdown()
{
	lib::LockGuard lock(m_assetsSystemLock);

	if (!m_loadedAssets.empty())
	{
		const lib::DynamicArray<AssetHandle> loadedAssets = GetLoadedAssetsList_Locked();

		for (const AssetHandle& asset : loadedAssets)
		{
			SPT_LOG_ERROR(AssetsSystem, "Asset was not unloaded! Name: {}", asset->GetName().ToString());
		}

		SPT_CHECK_NO_ENTRY_MSG("Not all assets were unloaded before shutdown!");
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

	SPT_CHECK_MSG(assetInstance->GetBlackboard().GetUnloadedTypes().empty(), "Asset data was unloaded during creation. This is not allowed.");

	SaveAssetImpl(assetInstance);

	m_loadedAssets[initializer.path] = assetInstance.Get();

	assetInstance->PostInitialize();

	return CreateResult(std::move(assetInstance));
}

LoadResult AssetsSystem::LoadAsset(const ResourcePath& path)
{
	SPT_PROFILER_FUNCTION();

	lib::LockGuard lock(m_assetsSystemLock);

	{
		const auto it = m_loadedAssets.find(path);
		if (it != m_loadedAssets.end())
		{
			return LoadResult(AssetHandle(it->second));
		}
	}

	const lib::Path fullPath = m_contentPath / path.GetPath();

	if (!std::filesystem::exists(fullPath))
	{
		return LoadResult(ELoadError::DoesNotExist);
	}

	AssetInstanceData assetData = std::move(ReadAssetData(fullPath));

	const AssetHandle assetInstance = CreateAssetInstance(AssetInitializer
														  {
															  .type = assetData.type,
															  .path = path,
														  });

	if (!assetInstance.IsValid())
	{
		return ELoadError::FailedToCreateInstance;
	}

	assetInstance->AssignData(std::move(assetData));

	m_loadedAssets[path] = assetInstance.Get();

	assetInstance->PostLoad();

	assetInstance->PostInitialize();

	return LoadResult(AssetHandle(std::move(assetInstance)));
}

AssetHandle AssetsSystem::LoadAssetChecked(const ResourcePath& path)
{
	SPT_PROFILER_FUNCTION();

	const LoadResult loadResult = LoadAsset(path);
	SPT_CHECK(loadResult.HasValue());

	return AssetHandle(loadResult.GetValue());
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

	return EDeleteResult::Success;
}

void AssetsSystem::SaveAsset(const AssetHandle& asset)
{
	SPT_PROFILER_FUNCTION();

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

	m_loadedAssets.erase(instance.GetPath());
}

void AssetsSystem::SaveAssetImpl(AssetHandle asset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(asset.IsValid());

	asset->PreSave();

	AssetBlackboard& assetRuntimeData = asset->GetBlackboard();

	// Reload unloaded types for serialization
	if (!assetRuntimeData.GetUnloadedTypes().empty())
	{
		const lib::Path fullPath = m_contentPath / asset->GetPath();
		AssetInstanceData assetSavedData = ReadAssetData(fullPath);

		for (lib::RuntimeTypeInfo unloadedDataType : assetRuntimeData.GetUnloadedTypes())
		{
			SPT_MAYBE_UNUSED
			const Bool success = assetRuntimeData.MoveType(assetSavedData.blackboard, unloadedDataType);

			SPT_CHECK(success);
		}
	}

	const lib::String data = srl::SerializationHelper::SerializeStruct(asset->GetInstanceData());

	// unload data that was unloaded again
	if (!assetRuntimeData.GetUnloadedTypes().empty())
	{
		for (lib::RuntimeTypeInfo unloadedDataType : assetRuntimeData.GetUnloadedTypes())
		{
			assetRuntimeData.Unload(unloadedDataType);
		}
	}

	const lib::Path fullPath = m_contentPath / asset->GetPath();
	std::ofstream stream = lib::File::OpenOutputStream(fullPath.string(), lib::EFileOpenFlags::ForceCreate);
	SPT_CHECK(stream.is_open());

	stream << data;

	stream.close();

	asset->PostSave();
}

AssetInstanceData AssetsSystem::ReadAssetData(const lib::Path& fullPath) const
{
	std::ifstream stream = lib::File::OpenInputStream(fullPath.string());

	if (!stream.is_open())
	{
		stream.close();
		return {};
	}

	std::stringstream stringStream;
	stringStream << stream.rdbuf();

	AssetInstanceData assetData;
	srl::SerializationHelper::DeserializeStruct(assetData, stringStream.str());

	stream.close();

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

} // spt::as
