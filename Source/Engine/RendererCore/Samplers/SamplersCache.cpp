#include "SamplersCache.h"
#include "Types/Sampler.h"

namespace spt::rdr
{

SamplersCache::SamplersCache()
{ }

void SamplersCache::Initialize()
{
	// Nothing for now
}

void SamplersCache::Uninitialize()
{
	const lib::LockGuard lockGuard(m_samplersPendingFlushLock);

	m_cachedSamplers.clear();
	m_samplersPendingFlush.clear();
}

void SamplersCache::FlushPendingSamplers()
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lockGuard(m_samplersPendingFlushLock);

	std::move(std::begin(m_samplersPendingFlush), std::end(m_samplersPendingFlush), std::inserter(m_cachedSamplers, std::end(m_cachedSamplers)));
	m_samplersPendingFlush.clear();
}

lib::SharedRef<Sampler> SamplersCache::GetOrCreateSampler(const rhi::SamplerDefinition& def)
{
	SPT_PROFILER_FUNCTION();

	if (lib::HasAnyFlag(def.flags, rhi::ESamplerFlags::NotCached))
	{
		return lib::MakeShared<Sampler>(def);
	}

	const SizeType hash = lib::GetHash(def);

	// first try to find sampler in cached array (lock-free)
	const auto foundCachedSampler = m_cachedSamplers.find(hash);
	if (foundCachedSampler != std::cend(m_cachedSamplers))
	{
		return lib::Ref(foundCachedSampler->second);
	}

	const lib::LockGuard lockGuard(m_samplersPendingFlushLock);

	lib::SharedPtr<Sampler>& createdSampler = m_samplersPendingFlush[hash];
	if (!createdSampler)
	{
		createdSampler = lib::MakeShared<Sampler>(def);
	}
	SPT_CHECK(!!createdSampler);

	return lib::Ref(createdSampler);
}

} // spt::rdr
