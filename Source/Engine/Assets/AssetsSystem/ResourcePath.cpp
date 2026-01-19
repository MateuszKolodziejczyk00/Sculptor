#include "ResourcePath.h"


namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// ResourcePathsCache ============================================================================

ResourcePathsCache& ResourcePathsCache::GetInstance()
{
	static ResourcePathsCache instance;
	return instance;
}

const CachedPath* ResourcePathsCache::FindCachedPath(ResourcePathID id) const
{
	const lib::LockGuard lock(m_lock);

	const auto found_path = m_paths.find(id);
	return found_path != m_paths.cend() ? found_path->second.get() : nullptr;
}

const CachedPath& ResourcePathsCache::GetCachedPathChecked(ResourcePathID id) const
{
	const CachedPath* pathPtr = FindCachedPath(id);
	SPT_CHECK(!!pathPtr);

	return *pathPtr;
}

const CachedPath& ResourcePathsCache::GetOrCreatePath(lib::Path path)
{
	path = path.lexically_normal();
	path.make_preferred();

	const ResourcePathID id = ComputePathID(path);

	const lib::LockGuard lock(m_lock);

	lib::UniquePtr<CachedPath>& cachedPath = m_paths[id];

	if (!cachedPath)
	{
		cachedPath = std::make_unique<CachedPath>();
		cachedPath->id   = id;
		cachedPath->path = std::move(path);
	}

	return *cachedPath;
}

ResourcePathID ResourcePathsCache::ComputePathID(const lib::Path& path) const
{
	return lib::GetHash(path);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ResourcePath ==================================================================================

ResourcePath::ResourcePath()
{
}

ResourcePath::ResourcePath(const char* path)
	: ResourcePath(lib::Path(path))
{
}

ResourcePath::ResourcePath(lib::Path path)
{
	m_cachedPath = &ResourcePathsCache::GetInstance().GetOrCreatePath(std::move(path));
}

lib::String ResourcePath::GetName() const
{
	SPT_CHECK(m_cachedPath->path.has_filename())

	return m_cachedPath->path.filename().string();
}

ResourcePath ResourcePath::GetCachedPath(ResourcePathID pathID)
{
	const CachedPath* cachedPath = ResourcePathsCache::GetInstance().FindCachedPath(pathID);
	return ResourcePath(cachedPath);
}

const lib::Path& ResourcePath::GetPath() const
{
	static lib::Path emptyPath;
	return m_cachedPath ? m_cachedPath->path : emptyPath;
}

} // spt::as
