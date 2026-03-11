#pragma once

#include "SculptorCoreTypes.h"
#include "GeometryPipeline.h"


namespace spt::rsc::depth_only
{

void RenderDepthOnly(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams);

} // spt::rsc::depth_only
