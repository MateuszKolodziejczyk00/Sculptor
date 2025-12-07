#include "AssetsDB.h"


namespace spt::as
{

AssetsDB::AssetsDB()
{
}

void AssetsDB::Initalize(const AssetsDBInitInfo& initInfo)
{
	SPT_PROFILER_FUNCTION();

	m_ddc = &initInfo.ddc;

	Bool shouldInitalizeDB = false;
	if (initInfo.ddc.DoesKeyExist(initInfo.ddcKey))
	{
		m_ddcHandle = initInfo.ddc.GetResourceHandle(initInfo.ddcKey, DDCResourceMapping{ .writable = true });
	}
	else
	{
		const Uint64 initialDescriptorsCapacity = 1024u;

		m_ddcHandle = initInfo.ddc.CreateDerivedData(initInfo.ddcKey, sizeof(AssetsDBHeader) + sizeof(PersistentAssetDescriptorData) * initialDescriptorsCapacity);
		shouldInitalizeDB = true;
	}

	const Uint64 storageDescriptorsNum = (m_ddcHandle.GetImmutableSpan().size() - sizeof(AssetsDBHeader)) / sizeof(PersistentAssetDescriptorData);

	m_ddcHeader      = reinterpret_cast<AssetsDBHeader*>(m_ddcHandle.GetMutablePtr());
	m_ddcDescriptors = lib::Span<PersistentAssetDescriptorData>(reinterpret_cast<PersistentAssetDescriptorData*>(m_ddcHandle.GetMutablePtr() + sizeof(AssetsDBHeader)), storageDescriptorsNum);

	if (shouldInitalizeDB)
	{
		m_ddcHeader->descriptorsNum = 0u;
	}

	for (Uint32 i = 0; i < m_ddcHeader->descriptorsNum; ++i)
	{
		const PersistentAssetDescriptorData& persistentDescriptor = m_ddcDescriptors[i];

		AssetDescriptor descriptor;
		descriptor.assetTypeKey = persistentDescriptor.assetTypeKey;
		m_descriptors.emplace(persistentDescriptor.pathID, RuntimeDescriptor{ .descriptor = descriptor, .ddcDescriptorIdx = i });
	}
}

void AssetsDB::Shutdown()
{
	SPT_PROFILER_FUNCTION();

	m_ddcHandle.Release();

	m_ddcHeader      = nullptr;
	m_ddcDescriptors = lib::Span<PersistentAssetDescriptorData>();
	m_ddc            = nullptr;

	m_descriptors.clear();
}

void AssetsDB::SaveAssetDescriptor(const ResourcePath& assetPath, const AssetDescriptor& descriptor)
{
	SPT_PROFILER_FUNCTION();

	const lib::WriteLockGuard lock(m_lock);

	const auto it = m_descriptors.find(assetPath.GetID());
	if (it != m_descriptors.end())
	{
		return;
	}

	const Uint32 descriptorIdx = m_ddcHeader->descriptorsNum;
	m_ddcHeader->descriptorsNum += 1u;

	if (descriptorIdx >= m_ddcDescriptors.size())
	{
		ResizeStorage_Locked(m_ddcDescriptors.size() * 2u);
	}

	m_ddcDescriptors[descriptorIdx] = PersistentAssetDescriptorData{ .pathID = assetPath.GetID(), .assetTypeKey = descriptor.assetTypeKey };

	m_descriptors[assetPath.GetID()] = RuntimeDescriptor{ .descriptor = descriptor, .ddcDescriptorIdx = descriptorIdx };

	m_ddcHandle.FlushWrites();
}

void AssetsDB::DeleteAssetDescriptor(const ResourcePath& assetPath)
{
	SPT_PROFILER_FUNCTION();

	const lib::WriteLockGuard lock(m_lock);

	const auto it = m_descriptors.find(assetPath.GetID());
	if (it == m_descriptors.end())
	{
		return;
	}

	const RuntimeDescriptor& descriptorToDelete = it->second;

	m_ddcHeader->descriptorsNum -= 1u;
	std::swap(m_ddcDescriptors[descriptorToDelete.ddcDescriptorIdx], m_ddcDescriptors[m_ddcHeader->descriptorsNum]);

	m_descriptors.erase(it);

	m_ddcHandle.FlushWrites();

	const PersistentAssetDescriptorData& swappedDescriptor = m_ddcDescriptors[descriptorToDelete.ddcDescriptorIdx];
	m_descriptors[swappedDescriptor.pathID].ddcDescriptorIdx = descriptorToDelete.ddcDescriptorIdx;
}

std::optional<AssetDescriptor> AssetsDB::GetAssetDescriptor(const ResourcePath& assetPath) const
{
	std::optional<AssetDescriptor> result;

	{
		const lib::ReadLockGuard lock(m_lock);

		auto it = m_descriptors.find(assetPath.GetID());
		if (it != m_descriptors.end())
		{
			result = it->second.descriptor;
		}
	}

	return result;
}

void AssetsDB::ResizeStorage_Locked(SizeType newSize)
{
	AssetsDBHeader oldHeader = *m_ddcHeader;
	lib::DynamicArray<PersistentAssetDescriptorData> oldDescriptors(m_ddcDescriptors.begin(), m_ddcDescriptors.begin() + oldHeader.descriptorsNum);

	const DerivedDataKey ddcKey = m_ddcHandle.GetKey();

	m_ddcHandle.Release();

	const Uint64 newTotalSize = sizeof(AssetsDBHeader) + sizeof(PersistentAssetDescriptorData) * newSize;
	m_ddcHandle = m_ddc->CreateDerivedData(ddcKey, newTotalSize);

	m_ddcHeader      = reinterpret_cast<AssetsDBHeader*>(m_ddcHandle.GetMutablePtr());
	m_ddcDescriptors = lib::Span<PersistentAssetDescriptorData>(reinterpret_cast<PersistentAssetDescriptorData*>(m_ddcHandle.GetMutablePtr() + sizeof(AssetsDBHeader)), newSize);

	*m_ddcHeader = oldHeader;
	std::memcpy(m_ddcDescriptors.data(), oldDescriptors.data(), sizeof(PersistentAssetDescriptorData) * oldHeader.descriptorsNum);

	m_ddcHandle.FlushWrites();
}

} // spt::as
