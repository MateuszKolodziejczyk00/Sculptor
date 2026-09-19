#pragma once

#include "RendererCoreMacros.h"
#include "RendererResource.h"
#include "RHIBridge/RHIPipelineImpl.h"
#include "ShaderMetaData.h"


namespace spt::rdr
{

class Shader;


class RENDERER_CORE_API Pipeline : public RendererResource<rhi::RHIPipeline>
{
protected:

	using ResourceType = RendererResource<rhi::RHIPipeline>;

public:

	Pipeline();

	const smd::ShaderMetaData& GetMetaData() const;

protected:

	void AppendToPipelineMetaData(const smd::ShaderMetaData& shaderMetaData);

private:

	smd::ShaderMetaData m_metaData;
};

} // spt::rdr
