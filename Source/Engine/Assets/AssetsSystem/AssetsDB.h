#pragma once

#include "AssetTypes.h"
#include "AssetsSystemMacros.h"
#include "ResourcePath.h"
#include "SculptorCoreTypes.h"


namespace spt::as
{

struct AssetDescriptor
{
	AssetTypeKey assetTypeKey = 0u;
};


struct AssetsDBInitInfo
{
	DDC&           ddc;
	DerivedDataKey assetsDDCKey;
	DerivedDataKey pathsDDCKey;
};


class AssetsDB
{
	struct AssetsDBHeader
	{
		Uint32 descriptorsNum;
		Uint32 padding = 0u;
	};
	
	struct PersistentAssetDescriptorData
	{
		ResourcePathID pathID;
		AssetTypeKey   assetTypeKey = 0u;
	};

	struct RuntimeAssetDescriptor
	{
		AssetDescriptor descriptor;
		Uint32 ddcDescriptorIdx = idxNone<Uint32>;
	};

	struct PersistentPathDescriptorData
	{
		static constexpr Uint32 s_maxLength = 256u;
		wchar_t path[s_maxLength];
	};

public:

	AssetsDB();

	void Initalize(const AssetsDBInitInfo& initInfo);
	void Shutdown();

	void SaveAssetDescriptor(const ResourcePath& assetPath, const AssetDescriptor& descriptor);
	void DeleteAssetDescriptor(const ResourcePath& assetPath);

	std::optional<AssetDescriptor> GetAssetDescriptor(ResourcePathID pathID) const;

	Bool ContainsAsset(ResourcePathID pathID) const;

	// Returns valid path only to compiled assets
	ResourcePath GetPath(ResourcePathID pathID) const;

	Bool ContainsPathsDB() const { return m_pathsDDCHandle.IsValid(); }

	Bool IsWritable() const { return ContainsPathsDB(); }

private:

	void ResizeStorage_Locked(SizeType newSize);

	mutable lib::ReadWriteLock m_lock;

	lib::HashMap<ResourcePathID, RuntimeAssetDescriptor> m_descriptors;

	DDC*              m_ddc = nullptr;
	DDCResourceHandle m_assetsDDCHandle;

	AssetsDBHeader*                          m_ddcHeader = nullptr;
	lib::Span<PersistentAssetDescriptorData> m_ddcDescriptors;


	lib::HashMap<ResourcePathID, ResourcePath> m_paths;

	DDCResourceHandle                       m_pathsDDCHandle;
	lib::Span<PersistentPathDescriptorData> m_ddcPaths;
};

} // spt::as
