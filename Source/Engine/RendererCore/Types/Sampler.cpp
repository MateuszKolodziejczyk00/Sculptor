#include "Sampler.h"

namespace spt::rdr
{

Sampler::Sampler(const rhi::SamplerDefinition& def)
{
	GetRHI().InitializeRHI(def);
}


} // spt::rdr
