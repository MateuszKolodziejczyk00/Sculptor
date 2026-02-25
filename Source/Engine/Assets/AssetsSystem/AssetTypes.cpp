#include "AssetTypes.h"
#include "AssetsSystem.h"


namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// AssetFactory ==================================================================================

AssetFactory& AssetFactory::GetInstance()
{
	static AssetFactory instance;
	return instance;
}

AssetHandle AssetFactory::CreateAsset(AssetsSystem& owningSystem, const AssetInstanceDefinition& definition)
{
	const auto it = m_assetTypes.find(definition.type.id);
	if (it == m_assetTypes.end())
	{
		return nullptr;
	}

	const auto& factory = it->second.factory;
	return factory(owningSystem, definition);
}

void AssetFactory::DeleteAsset(AssetType assetType, AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data)
{
	const auto it = m_assetTypes.find(assetType.id);
	if (it == m_assetTypes.end())
	{
		return;
	}

	const auto& deleter = it->second.deleter;
	deleter(assetSystem, path, data);
}

AssetType AssetFactory::GetAssetTypeByKey(AssetTypeKey key) const
{
	const auto it = m_assetTypes.find(key);
	if (it == m_assetTypes.end())
	{
		return AssetType();
	}
	return it->second.type;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// AssetInstance =================================================================================

AssetInstance::~AssetInstance()
{
	m_owningSystem.OnAssetUnloaded(*this);
}

Bool AssetInstance::SetPermanent()
{
	const EAssetRuntimeFlags::Type prevFlags = AddRuntimeFlag(EAssetRuntimeFlags::Permanent);
	if ((prevFlags & EAssetRuntimeFlags::Permanent) == EAssetRuntimeFlags::None)
	{
		AddRef();
		return true;
	}
	return false;
}

Bool AssetInstance::ClearPermanent()
{
	const EAssetRuntimeFlags::Type prevFlags = RemoveRuntimeFlag(EAssetRuntimeFlags::Permanent);
	if ((prevFlags & EAssetRuntimeFlags::Permanent) != EAssetRuntimeFlags::None)
	{
		Release();
		return true;
	}
	return false;
}

void AssetInstance::SaveAsset()
{
	m_owningSystem.SaveAsset(AssetHandle(this));
}

lib::Path AssetInstance::GetRelativePath() const
{
	const ResourcePath path = m_owningSystem.ResolvePath(GetResourcePathID());
	SPT_CHECK(path.IsValid());
	return path.GetPath();
}

lib::Path AssetInstance::GetDirectoryPath() const
{
	return GetOwningSystem().GetContentPath() / GetRelativePath().parent_path();
}

ResourcePath AssetInstance::ResolveAssetRelativePath(const lib::Path& relativePath) const
{
	return (GetRelativePath().parent_path() / relativePath);
}

void AssetInstance::Initialize()
{
	OnInitialize();

	AddRuntimeFlag(EAssetRuntimeFlags::Initialized);
	js::JobInstance* initJobInstance = m_initializationJob.load();
	SPT_CHECK(!!initJobInstance);
	m_initializationJob.store(nullptr);

	initJobInstance->Release();
}

void AssetInstance::AssignInitializationJob(js::Job job)
{
	SPT_CHECK(!IsInitialized());
	SPT_CHECK(m_initializationJob.load() == nullptr);

	js::JobInstance* jobInstance = job.GetJobInstance().Get();
	jobInstance->AddRef();
	m_initializationJob.store(jobInstance);
}

} // spt::as
