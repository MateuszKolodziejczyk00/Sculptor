#pragma once

#include "RHICore/RHITextureTypes.h"


namespace spt::rsc::sr_denoiser
{
using RTSphericalBasisType = float;
constexpr rhi::EFragmentFormat RTSphericalBasisFormat = rhi::EFragmentFormat::R16_S_Float;

} // spt::rsc::sr_denoiser