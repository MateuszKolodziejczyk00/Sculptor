#include "ComputePipeline.h"

namespace spt::rdr
{

ComputePipeline::ComputePipeline(const RendererResourceName& name, const lib::SharedPtr<Shader>& shader)
	: Pipeline(shader)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK_NO_ENTRY();
}

} // spt::renderer
