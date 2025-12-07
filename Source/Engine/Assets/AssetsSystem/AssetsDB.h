#pragma once

#include "AssetsSystemMacros.h"
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
	DerivedDataKey ddcKey;
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

	struct RuntimeDescriptor
	{
		AssetDescriptor descriptor;
		Uint32 ddcDescriptorIdx = idxNone<Uint32>;
	};

public:

	AssetsDB();

	void Initalize(const AssetsDBInitInfo& initInfo);
	void Shutdown();

	void SaveAssetDescriptor(const ResourcePath& assetPath, const AssetDescriptor& descriptor);
	void DeleteAssetDescriptor(const ResourcePath& assetPath);

	std::optional<AssetDescriptor> GetAssetDescriptor(const ResourcePath& assetPath) const;

private:

	void ResizeStorage_Locked(SizeType newSize);

	mutable lib::ReadWriteLock m_lock;

	lib::HashMap<ResourcePathID, RuntimeDescriptor> m_descriptors;

	DDC*              m_ddc = nullptr;
	DDCResourceHandle m_ddcHandle;

	AssetsDBHeader*                          m_ddcHeader = nullptr;
	lib::Span<PersistentAssetDescriptorData> m_ddcDescriptors;
};

} // spt::as