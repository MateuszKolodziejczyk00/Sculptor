#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIShaderModuleImpl.h"
#include "ShaderMetaData.h"


namespace spt::rdr
{

struct RendererResourceName;

class RENDERER_TYPES_API Shader
{
public:

	Shader(const RendererResourceName& name, const lib::DynamicArray<rhi::ShaderModuleDefinition>& moduleDefinitions, const lib::SharedRef<smd::ShaderMetaData>& metaData);
	~Shader();

	const lib::DynamicArray<rhi::RHIShaderModule>&	GetShaderModules() const;
	const lib::SharedRef<smd::ShaderMetaData>&		GetMetaData() const;
	rhi::EPipelineType								GetPipelineType() const;

private:

	rhi::EPipelineType SelectPipelineType(const lib::DynamicArray<rhi::ShaderModuleDefinition>& moduleDefinitions) const;

	lib::DynamicArray<rhi::RHIShaderModule>	m_shaderModules;
	lib::SharedRef<smd::ShaderMetaData>		m_metaData;
	rhi::EPipelineType						m_pipelineType;
};

} // spt::rdr