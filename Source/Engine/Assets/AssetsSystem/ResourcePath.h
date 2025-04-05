#pragma once

#include "SculptorCoreTypes.h"
#include "FileSystem/File.h"
#include "AssetsSystemMacros.h"


namespace spt::as
{

using ResourcePathID = SizeType;
constexpr ResourcePathID InvalidResourcePathID = maxValue<SizeType>;


struct CachedPath
{
	lib::Path      path;
	ResourcePathID id;
};


class ResourcePathsCache
{
public:

	static ResourcePathsCache& GetInstance();

	const CachedPath& GetCachedPathChecked(ResourcePathID id) const;

	const CachedPath& GetOrCreatePath(lib::Path path);

private:

	ResourcePathsCache() = default;

	ResourcePathID ComputePathID(const lib::Path& path) const;

	mutable lib::Spinlock m_lock;
	lib::HashMap<ResourcePathID, lib::UniquePtr<CachedPath>> m_paths;
};


class ASSETS_SYSTEM_API ResourcePath
{
public:

	ResourcePath();
	ResourcePath(const char* path);
	ResourcePath(lib::Path path);

	ResourcePath(const ResourcePath&) = default;
	ResourcePath(ResourcePath&&)      = default;

	ResourcePath& operator=(const ResourcePath&) = default;
	ResourcePath& operator=(ResourcePath&&)      = default;

	Bool operator==(const ResourcePath& other) const
	{
		return m_cachedPath == other.m_cachedPath;
	}

	const lib::Path& GetPath() const;

	lib::String GetName() const;

	Bool IsValid() const { return !!m_cachedPath; }

	ResourcePathID GetID() const { return m_cachedPath ? m_cachedPath->id : InvalidResourcePathID; }

private:

	const CachedPath* m_cachedPath;
};

} // spt::as

namespace spt::lib
{

template<>
struct Hasher<as::ResourcePath>
{
	SizeType operator()(const as::ResourcePath& path) const
	{
		return lib::GetHash(path.GetID());
	}
};

} // spt::lib