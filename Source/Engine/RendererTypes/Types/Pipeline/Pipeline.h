#pragma once

#include "RendererTypesMacros.h"
#include "ShaderMetaData.h"
#include "RHICore/RHIPipelineLayoutTypes.h"


namespace spt::rdr
{

class Shader;


class RENDERER_TYPES_API Pipeline abstract
{
public:

	Pipeline(const lib::SharedRef<Shader>& shader);

	SPT_NODISCARD const lib::SharedRef<smd::ShaderMetaData>& GetMetaData() const;

protected:

	SPT_NODISCARD rhi::PipelineLayoutDefinition CreateLayoutDefinition(const smd::ShaderMetaData& metaData) const;

private:

	lib::SharedRef<smd::ShaderMetaData> m_metaData;
};

} // spt::rdr
