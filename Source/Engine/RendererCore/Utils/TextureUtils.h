#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHISynchronizationTypes.h"


namespace spt::rdr
{

class CommandRecorder;
class Texture;

namespace utils
{

void RENDERER_CORE_API GenerateMipMaps(rdr::CommandRecorder& recorder, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, Uint32 arrayLayer, const rhi::BarrierTextureTransitionDefinition& destTransition);

} // utils

} // spt::rdr