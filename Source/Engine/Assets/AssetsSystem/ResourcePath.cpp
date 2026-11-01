#include "ResourcePath.h"


namespace spt::as
{

namespace priv
{

SizeType CanonicalizePath(lib::StringView path, char* out)
{
	SizeType length       = 0u;
	SizeType segmentBegin = 0u;

	for (SizeType i = 0u; i <= path.size(); ++i)
	{
		const char c = i < path.size() ? path[i] : '/';
		if (c != '/' && c != '\\')
		{
			out[length++] = c;
			continue;
		}

		const lib::StringView segment(out + segmentBegin, length - segmentBegin);
		if (segment.empty() || segment == ".")
		{
			length = segmentBegin;
		}
		else if (segment == "..")
		{
			SPT_CHECK_MSG(segmentBegin > 0u, "Path escapes content root: {}", path);
			length = lib::StringView(out, segmentBegin - 1u).rfind('/') + 1u; // npos + 1 == 0
		}
		else
		{
			out[length++] = '/';
		}
		segmentBegin = length;
	}

	return length > 0u ? length - 1u : 0u;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// ResourcePathsCache ============================================================================

ResourcePathsCache& ResourcePathsCache::GetInstance()
{
	static ResourcePathsCache instance;
	return instance;
}

ResourcePathsCache::ResourcePathsCache()
	: m_arena("ResourcePathsArena", 64u * 1024u, 16u * 1024u * 1024u)
{
}

const CachedPath* ResourcePathsCache::FindCachedPath(ResourcePathID id) const
{
	const lib::ReadLockGuard lock(m_lock);

	const auto found_path = m_paths.find(id);
	return found_path != m_paths.cend() ? found_path->second : nullptr;
}

const CachedPath& ResourcePathsCache::GetCachedPathChecked(ResourcePathID id) const
{
	const CachedPath* pathPtr = FindCachedPath(id);
	SPT_CHECK(!!pathPtr);

	return *pathPtr;
}

const CachedPath& ResourcePathsCache::GetOrCreatePath(lib::StringView path)
{
	SPT_CHECK_MSG(path.size() < s_maxPathLength, "Path is too long: {}", path);

	char canonicalPathBuffer[s_maxPathLength];
	const lib::StringView canonicalPath(canonicalPathBuffer, priv::CanonicalizePath(path, canonicalPathBuffer));

	const ResourcePathID id = lib::FNV1a::Hash({ canonicalPath.data(), canonicalPath.size() });

	{
		const lib::ReadLockGuard lock(m_lock);

		const auto found_path = m_paths.find(id);
		if (found_path != m_paths.cend())
		{
			SPT_CHECK_MSG(found_path->second->path == canonicalPath, "ResourcePathID collision: '{}' and '{}'", found_path->second->path, canonicalPath);
			return *found_path->second;
		}
	}

	const lib::WriteLockGuard lock(m_lock);

	const CachedPath*& cachedPath = m_paths[id];

	if (!cachedPath)
	{
		char* pathData = reinterpret_cast<char*>(m_arena.Allocate(canonicalPath.size(), alignof(char)));
		std::memcpy(pathData, canonicalPath.data(), canonicalPath.size());

		cachedPath = m_arena.Allocate<CachedPath>(CachedPath{ .path = lib::StringView(pathData, canonicalPath.size()), .id = id });
	}

	return *cachedPath;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ResourcePath ==================================================================================

ResourcePath::ResourcePath()
{
}

ResourcePath::ResourcePath(lib::StringView path)
	: m_cachedPath(&ResourcePathsCache::GetInstance().GetOrCreatePath(path))
{
}

ResourcePath::ResourcePath(const char* path)
	: ResourcePath(lib::StringView(path))
{
}

ResourcePath::ResourcePath(const lib::String& path)
	: ResourcePath(lib::StringView(path))
{
}

ResourcePath::ResourcePath(const lib::Path& path)
	: ResourcePath(lib::StringView(path.generic_string()))
{
}

lib::StringView ResourcePath::GetName() const
{
	const lib::StringView path = GetPath();
	SPT_CHECK(!path.empty());

	return path.substr(path.rfind('/') + 1u); // npos + 1 == 0
}

ResourcePath ResourcePath::GetCachedPath(ResourcePathID pathID)
{
	const CachedPath* cachedPath = ResourcePathsCache::GetInstance().FindCachedPath(pathID);
	return ResourcePath(cachedPath);
}

lib::StringView ResourcePath::GetPath() const
{
	return m_cachedPath ? m_cachedPath->path : lib::StringView();
}

} // spt::as
