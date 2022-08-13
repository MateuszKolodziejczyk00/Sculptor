#pragma once

#include "RendererTypesMacros.h"
#include "RHIBridge/RHIShaderModuleImpl.h"
#include "RendererResource.h"


namespace spt::rhi
{
struct ShaderModuleDefinition;
}


namespace spt::renderer
{

struct RendererResourceName;


// Shader modules are not used in commands and can be destroyed right after pipeline creation, so don't use deferred rhi release
class RENDERER_TYPES_API ShaderModule : public RendererResource<rhi::RHIShaderModule, false>
{
protected:

	using ResourceType = RendererResource<rhi::RHIShaderModule, false>;

public:

	ShaderModule(const RendererResourceName& name, const rhi::ShaderModuleDefinition& definition);
};

}
