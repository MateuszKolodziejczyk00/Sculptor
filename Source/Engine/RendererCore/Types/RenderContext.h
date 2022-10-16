#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIRenderContextImpl.h"
#include "RendererResource.h"
#include "DescriptorSets/DynamicDescriptorSetsManager.h"


namespace spt::rdr
{

struct RendererResourceName;


class RENDERER_CORE_API RenderContext : public RendererResource<rhi::RHIRenderContext>
{
protected:

	using ResourceType = RendererResource<rhi::RHIRenderContext>;

public:

	RenderContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef);

	void BuildDescriptorSets(const lib::DynamicArray<DynamicDescriptorSetInfo>& setInfos);
	rhi::RHIDescriptorSet GetDescriptorSet(DSStateID id) const;

private:

	DynamicDescriptorSetsManager m_dynamicDescriptorSets;
};

} // spt::rdr