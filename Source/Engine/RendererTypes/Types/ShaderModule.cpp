#include "ShaderModule.h"
#include "RendererUtils.h"

namespace spt::renderer
{

ShaderModule::ShaderModule(const RendererResourceName& name, const rhi::ShaderModuleDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	GetRHI().InitializeRHI(definition);
	GetRHI().SetName(name.Get());
}

}
