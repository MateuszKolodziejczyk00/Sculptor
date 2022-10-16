#include "RenderContext.h"
#include "RendererUtils.h"

namespace spt::rdr
{

RenderContext::RenderContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(contextDef);
	GetRHI().SetName(name.Get());
}

void RenderContext::BuildDescriptorSets(const lib::DynamicArray<DynamicDescriptorSetInfo>& setInfos)
{
	m_dynamicDescriptorSets.BuildDescriptorSets(*this, setInfos);
}

rhi::RHIDescriptorSet RenderContext::GetDescriptorSet(DSStateID id) const
{
	return m_dynamicDescriptorSets.GetDescriptorSet(id);
}

} // spt::rdr
