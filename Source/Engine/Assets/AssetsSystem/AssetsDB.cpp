#include "AssetsDB.h"
#include "Containers/DynamicArray.h"
#include "Eigen/src/SparseCore/SparseUtil.h"
#include "ResourcePath.h"
#include <cstring>
#include <cwchar>


namespace spt::as
{

AssetsDB::AssetsDB()
{
}

void AssetsDB::Initalize(const AssetsDBInitInfo& initInfo)
{
	SPT_PROFILER_FUNCTION();

	m_ddc = &initInfo.ddc;

	const Bool shouldLoadPathsDB = initInfo.pathsDDCKey.IsValid();

	Bool shouldInitalizeDB = false;
	if (initInfo.ddc.DoesKeyExist(initInfo.assetsDDCKey) || initInfo.ddc.DoesKeyExist(initInfo.pathsDDCKey))
	{
		m_assetsDDCHandle = initInfo.ddc.GetResourceHandle(initInfo.assetsDDCKey, DDCResourceMapping{ .writable = true });

		if (shouldLoadPathsDB)
		{
			m_pathsDDCHandle = initInfo.ddc.GetResourceHandle(initInfo.pathsDDCKey, DDCResourceMapping{ .writable = true });
		}
	}
	else
	{
		const Uint64 initialDescriptorsCapacity = 1024u;

		m_assetsDDCHandle = initInfo.ddc.CreateDerivedData(initInfo.assetsDDCKey, sizeof(AssetsDBHeader) + sizeof(PersistentAssetDescriptorData) * initialDescriptorsCapacity);

		if (shouldLoadPathsDB)
		{
			m_pathsDDCHandle = initInfo.ddc.CreateDerivedData(initInfo.pathsDDCKey, sizeof(PersistentPathDescriptorData) * initialDescriptorsCapacity);
		}

		shouldInitalizeDB = true;
	}


	const Uint64 storageDescriptorsNum = (m_assetsDDCHandle.GetImmutableSpan().size() - sizeof(AssetsDBHeader)) / sizeof(PersistentAssetDescriptorData);

	m_ddcHeader      = reinterpret_cast<AssetsDBHeader*>(m_assetsDDCHandle.GetMutablePtr());
	m_ddcDescriptors = lib::Span<PersistentAssetDescriptorData>(reinterpret_cast<PersistentAssetDescriptorData*>(m_assetsDDCHandle.GetMutablePtr() + sizeof(AssetsDBHeader)), storageDescriptorsNum);

	if (shouldInitalizeDB)
	{
		m_ddcHeader->descriptorsNum = 0u;
	}

	m_descriptors.reserve(m_ddcHeader->descriptorsNum);

	if (shouldLoadPathsDB)
	{
		SPT_CHECK_MSG((m_pathsDDCHandle.GetSize() / sizeof(PersistentPathDescriptorData)) == storageDescriptorsNum, "Assets and Paths databases do not match!")
		m_ddcPaths       = lib::Span<PersistentPathDescriptorData>(reinterpret_cast<PersistentPathDescriptorData*>(m_pathsDDCHandle.GetMutablePtr()), storageDescriptorsNum);
		m_paths.reserve(m_ddcHeader->descriptorsNum);
	}

	for (Uint32 i = 0; i < m_ddcHeader->descriptorsNum; ++i)
	{
		const PersistentAssetDescriptorData& persistentAssetDescriptor = m_ddcDescriptors[i];

		AssetDescriptor descriptor;
		descriptor.assetTypeKey = persistentAssetDescriptor.assetTypeKey;
		m_descriptors.emplace(persistentAssetDescriptor.pathID, RuntimeAssetDescriptor{ .descriptor = descriptor, .ddcDescriptorIdx = i });

		if (shouldLoadPathsDB)
		{
			const PersistentPathDescriptorData& persistentPathDescriptor = m_ddcPaths[i];
			m_paths.emplace(persistentAssetDescriptor.pathID, lib::Path(persistentPathDescriptor.path));
		}
	}

	SPT_CHECK(shouldLoadPathsDB == ContainsPathsDB());
}

void AssetsDB::Shutdown()
{
	SPT_PROFILER_FUNCTION();

	m_assetsDDCHandle.Release();

	m_ddcHeader      = nullptr;
	m_ddcDescriptors = {};
	m_ddc            = nullptr;

	m_descriptors.clear();

	m_pathsDDCHandle.Release();
	m_ddcPaths = {};
	m_paths.clear();
}

void AssetsDB::SaveAssetDescriptor(const ResourcePath& assetPath, const AssetDescriptor& descriptor)
{
	SPT_PROFILER_FUNCTION();

	const lib::WriteLockGuard lock(m_lock);

	SPT_CHECK(IsWritable());

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

	PersistentPathDescriptorData& pathPersistentDescriptor = m_ddcPaths[descriptorIdx];
	const wchar_t* path = assetPath.GetPath().c_str();
	const SizeType pathLength = std::wcslen(path);
	std::memcpy(pathPersistentDescriptor.path, path, pathLength * sizeof(wchar_t));
	std::memset(pathPersistentDescriptor.path + pathLength, 0, PersistentPathDescriptorData::s_maxLength - pathLength * sizeof(wchar_t));

	m_descriptors[assetPath.GetID()] = RuntimeAssetDescriptor{ .descriptor = descriptor, .ddcDescriptorIdx = descriptorIdx };
	m_paths[assetPath.GetID()] = assetPath;

	m_assetsDDCHandle.FlushWrites();
	m_pathsDDCHandle.FlushWrites();
}

void AssetsDB::DeleteAssetDescriptor(ResourcePathID pathID)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsWritable());

	const lib::WriteLockGuard lock(m_lock);

	const auto it = m_descriptors.find(pathID);
	if (it == m_descriptors.end())
	{
		return;
	}

	const RuntimeAssetDescriptor& descriptorToDelete = it->second;

	m_ddcHeader->descriptorsNum -= 1u;

	if (m_ddcHeader->descriptorsNum != descriptorToDelete.ddcDescriptorIdx)
	{
		std::swap(m_ddcDescriptors[descriptorToDelete.ddcDescriptorIdx], m_ddcDescriptors[m_ddcHeader->descriptorsNum]);
		std::swap(m_ddcPaths[descriptorToDelete.ddcDescriptorIdx], m_ddcPaths[m_ddcHeader->descriptorsNum]);

		const PersistentAssetDescriptorData& swappedDescriptor = m_ddcDescriptors[descriptorToDelete.ddcDescriptorIdx];
		m_descriptors[swappedDescriptor.pathID].ddcDescriptorIdx = descriptorToDelete.ddcDescriptorIdx;
	}
	else
	{
		std::memset(&m_ddcDescriptors[descriptorToDelete.ddcDescriptorIdx], 0, sizeof(PersistentAssetDescriptorData));
		std::memset(&m_ddcPaths[descriptorToDelete.ddcDescriptorIdx], 0, sizeof(PersistentPathDescriptorData));
	}

	m_descriptors.erase(it);
	m_assetsDDCHandle.FlushWrites();

	m_paths.erase(pathID);
	m_pathsDDCHandle.FlushWrites();
}

std::optional<AssetDescriptor> AssetsDB::GetAssetDescriptor(ResourcePathID pathID) const
{
	std::optional<AssetDescriptor> result;

	{
		const lib::ReadLockGuard lock(m_lock);

		auto it = m_descriptors.find(pathID);
		if (it != m_descriptors.end())
		{
			result = it->second.descriptor;
		}
	}

	return result;
}

lib::DynamicArray<AssetMetaData> AssetsDB::GetAllAssetsDescriptors() const
{
	lib::DynamicArray<AssetMetaData> result;

	{
		const lib::ReadLockGuard lock(m_lock);

		result.reserve(m_descriptors.size());
		for (const auto& [pathID, runtimeDescriptor] : m_descriptors)
		{
			result.emplace_back(AssetMetaData{ .pathID = pathID, .descriptor = runtimeDescriptor.descriptor });
		}
	}

	return result;
}

Bool AssetsDB::ContainsAsset(ResourcePathID pathID) const
{
	const lib::ReadLockGuard lock(m_lock);

	return m_descriptors.contains(pathID);
}

ResourcePath AssetsDB::GetPath(ResourcePathID pathID) const
{
	SPT_CHECK(ContainsPathsDB());

	ResourcePath result;

	{
		const lib::ReadLockGuard lock(m_lock);

		auto it = m_paths.find(pathID);
		if (it != m_paths.end())
		{
			result = it->second;
		}
	}

	return result;
}

void AssetsDB::ResizeStorage_Locked(SizeType newSize)
{
	SPT_CHECK(IsWritable());

	AssetsDBHeader oldHeader = *m_ddcHeader;
	lib::DynamicArray<PersistentAssetDescriptorData> oldDescriptors(m_ddcDescriptors.begin(), m_ddcDescriptors.begin() + oldHeader.descriptorsNum);
	lib::DynamicArray<PersistentPathDescriptorData> oldPaths(m_ddcPaths.begin(), m_ddcPaths.begin() + oldHeader.descriptorsNum);

	SPT_CHECK(newSize >= oldHeader.descriptorsNum);

	const DerivedDataKey assetsDDCKey = m_assetsDDCHandle.GetKey();
	const DerivedDataKey pathsDDCKey  = m_pathsDDCHandle.GetKey();

	m_assetsDDCHandle.Release();
	m_pathsDDCHandle.Release();

	const Uint64 newAssetsDCCTotalSize = sizeof(AssetsDBHeader) + sizeof(PersistentAssetDescriptorData) * newSize;
	m_assetsDDCHandle = m_ddc->CreateDerivedData(assetsDDCKey, newAssetsDCCTotalSize);

	m_ddcHeader      = reinterpret_cast<AssetsDBHeader*>(m_assetsDDCHandle.GetMutablePtr());
	m_ddcDescriptors = lib::Span<PersistentAssetDescriptorData>(reinterpret_cast<PersistentAssetDescriptorData*>(m_assetsDDCHandle.GetMutablePtr() + sizeof(AssetsDBHeader)), newSize);

	*m_ddcHeader = oldHeader;
	std::memcpy(m_ddcDescriptors.data(), oldDescriptors.data(), sizeof(PersistentAssetDescriptorData) * oldHeader.descriptorsNum);

	const Uint64 newPathsDCCTotalSize = sizeof(PersistentPathDescriptorData) * newSize;
	m_pathsDDCHandle = m_ddc->CreateDerivedData(pathsDDCKey, newPathsDCCTotalSize);

	m_ddcPaths = lib::Span<PersistentPathDescriptorData>(reinterpret_cast<PersistentPathDescriptorData*>(m_pathsDDCHandle.GetMutablePtr()), newSize);

	std::memcpy(m_ddcPaths.data(), oldPaths.data(), sizeof(PersistentPathDescriptorData) * oldHeader.descriptorsNum);

	m_assetsDDCHandle.FlushWrites();
	m_pathsDDCHandle.FlushWrites();
}

} // spt::as
