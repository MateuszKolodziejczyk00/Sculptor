#pragma once

#include "RendererTypesMacros.h"
#include "RHIBridge/RHIShaderModuleImpl.h"


namespace spt::rhi
{
struct ShaderModuleDefinition;
}


namespace spt::renderer
{

struct RendererResourceName;


class RENDERER_TYPES_API ShaderModule
{
public:

	ShaderModule(const RendererResourceName& name, const rhi::ShaderModuleDefinition& definition);
	~ShaderModule();

	rhi::RHIShaderModule&			GetRHI();
	const rhi::RHIShaderModule&		GetRHI() const;

private:

	rhi::RHIShaderModule			m_rhiModule;
};

}
