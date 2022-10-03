#pragma once

#include "RendererCoreMacros.h"
#include "RendererResource.h"
#include "RHIBridge/RHIPipelineImpl.h"
#include "ShaderMetaData.h"
#include "RHICore/RHIPipelineLayoutTypes.h"


namespace spt::rdr
{

class Shader;


class RENDERER_CORE_API Pipeline : public RendererResource<rhi::RHIPipeline>
{
protected:

	using ResourceType = RendererResource<rhi::RHIPipeline>;

public:

	Pipeline(const lib::SharedRef<Shader>& shader);

	SPT_NODISCARD const lib::SharedRef<smd::ShaderMetaData>& GetMetaData() const;

protected:

	SPT_NODISCARD rhi::PipelineLayoutDefinition CreateLayoutDefinition(const smd::ShaderMetaData& metaData) const;

private:

	lib::SharedRef<smd::ShaderMetaData> m_metaData;
};

} // spt::rdr
