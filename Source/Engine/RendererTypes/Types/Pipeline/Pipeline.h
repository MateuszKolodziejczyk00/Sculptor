#pragma once

#include "RendererTypesMacros.h"
#include "ShaderMetaData.h"
#include "RHICore/RHIPipelineLayoutTypes.h"


namespace spt::renderer
{

class Shader;


class Pipeline abstract
{
public:

	Pipeline(const lib::SharedPtr<Shader>& shader);

private:

	rhi::PipelineLayoutDefinition CreateLayoutDefinition(const smd::ShaderMetaData& metaData) const;

	lib::SharedPtr<smd::ShaderMetaData> m_metaData;
};

} // spt::renderer
