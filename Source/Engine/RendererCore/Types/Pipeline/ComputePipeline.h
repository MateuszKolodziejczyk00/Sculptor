#pragma once

#include "Pipeline.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class RENDERER_CORE_API ComputePipeline : public Pipeline
{
public:

	ComputePipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader);
};

} // spt::rdr
