#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHISynchronizationTypes.h"

namespace spt::rdr
{
class CommandRecorder;
class Texture;
} // spt::rdr


namespace spt::gfx
{

void GRAPHICS_API GenerateMipMaps(rdr::CommandRecorder& recorder, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, Uint32 arrayLayer, const rhi::BarrierTextureTransitionDefinition& destTransition);

} // spt::gfx