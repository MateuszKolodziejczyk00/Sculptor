#pragma once

#include "RendererTypesMacros.h"
#include "ShaderMetaData.h"
#include "RHICore/RHIPipelineLayoutTypes.h"


namespace spt::renderer
{

class Shader;


class RENDERER_TYPES_API Pipeline abstract
{
public:

	Pipeline(const lib::SharedPtr<Shader>& shader);

	SPT_NODISCARD const lib::SharedPtr<smd::ShaderMetaData>& GetMetaData() const;

protected:

	SPT_NODISCARD rhi::PipelineLayoutDefinition CreateLayoutDefinition(const smd::ShaderMetaData& metaData) const;

private:

	lib::SharedPtr<smd::ShaderMetaData> m_metaData;
};

} // spt::renderer
