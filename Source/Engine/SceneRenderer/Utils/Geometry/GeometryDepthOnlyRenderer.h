#pragma once

#include "SculptorCoreTypes.h"
#include "GeometryPipeline.h"


namespace spt::rsc::depth_only
{

gp::GeometryPipelineExecutor* CreatePipelineExecutor(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams);
void                          FinishPipelineExecution(rg::RenderGraphBuilder& graphBuilder, gp::GeometryPipelineExecutor* executor);

} // spt::rsc::depth_only
