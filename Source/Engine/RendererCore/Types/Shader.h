#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIShaderModuleImpl.h"
#include "RendererResource.h"
#include "ShaderMetaData.h"


namespace spt::rdr
{

struct RendererResourceName;


class RENDERER_CORE_API Shader : public rdr::RendererResource<rhi::RHIShaderModule>
{
protected:

	using ResourceType = RendererResource<rhi::RHIShaderModule>;

public:

	Shader(const RendererResourceName& name, const rhi::ShaderModuleDefinition& moduleDef, smd::ShaderMetaData&& metaData);

	const smd::ShaderMetaData&	GetMetaData() const;

	rhi::EPipelineType			GetPipelineType() const;
	rhi::EShaderStage			GetStage() const;

private:

	smd::ShaderMetaData	m_metaData;
	rhi::EPipelineType	m_pipelineType;
};

} // spt::rdr