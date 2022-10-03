#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"

namespace spt::rhi
{
struct SamplerDefinition;
} // spt::rhi


namespace spt::rdr
{

class Sampler;


class SamplersCache
{
public:

	SamplersCache();

	void Initialize();
	void Uninitialize();

	void FlushPendingSamplers();

	lib::SharedRef<Sampler> GetOrCreateSampler(const rhi::SamplerDefinition& def);

private:

	lib::HashMap<SizeType, lib::SharedPtr<Sampler>> m_cachedSamplers;

	lib::Lock m_samplersPendingFlushLock;
	lib::HashMap<SizeType, lib::SharedPtr<Sampler>> m_samplersPendingFlush;
};

} // spt::rdr
