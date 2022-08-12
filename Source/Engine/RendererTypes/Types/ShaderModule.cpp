#include "ShaderModule.h"
#include "RendererUtils.h"

namespace spt::renderer
{

ShaderModule::ShaderModule(const RendererResourceName& name, const rhi::ShaderModuleDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	m_rhiModule.InitializeRHI(definition);
	m_rhiModule.SetName(name.Get());
}

ShaderModule::~ShaderModule()
{
	// Shader modules are not used in commands and can be destroyed right after pipeline creation, so we can destroy it right away
	m_rhiModule.ReleaseRHI();
}

rhi::RHIShaderModule& ShaderModule::GetRHI()
{
	return m_rhiModule;
}

const rhi::RHIShaderModule& ShaderModule::GetRHI() const
{
	return m_rhiModule;
}

}
