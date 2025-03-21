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

const CachedPath& ResourcePathsCache::GetCachedPathChecked(ResourcePathID id) const
{
	const lib::LockGuard lock(m_lock);

	const lib::UniquePtr<CachedPath>& path = m_paths.at(id);

	SPT_CHECK(!!path);

	return *path;
}

const CachedPath& ResourcePathsCache::GetOrCreatePath(lib::Path path)
{
	const lib::LockGuard lock(m_lock);

	const ResourcePathID id = ComputePathID(path);

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

const lib::Path& ResourcePath::GetPath() const
{
	static lib::Path emptyPath;
	return m_cachedPath ? m_cachedPath->path : emptyPath;
}

} // spt::as
