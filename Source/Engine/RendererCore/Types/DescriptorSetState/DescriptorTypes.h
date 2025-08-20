#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "Utility/NamedHandle.h"


namespace spt::rdr
{


using ResourceDescriptorHandle = lib::NamedHandle<Uint32, struct ResourceDescriptorParameter>;
using SamplerDescriptorHandle  = lib::NamedHandle<Uint32, struct SamplerDescriptorParameter>;

using ResourceDescriptorIdx = typename ResourceDescriptorHandle::DataType;
using SamplerDescriptorIdx  = typename SamplerDescriptorHandle::DataType;


static constexpr ResourceDescriptorIdx invalidResourceDescriptorIdx = ResourceDescriptorIdx(idxNone<Uint32>);
static constexpr SamplerDescriptorIdx  invalidSamplerDescriptorIdx  = SamplerDescriptorIdx(idxNone<Uint32>);

} // spt::rdr