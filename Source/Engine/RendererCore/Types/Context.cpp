#include "Context.h"
#include "RendererUtils.h"

namespace spt::rdr
{

Context::Context(const RendererResourceName& name, const rhi::ContextDefinition& contextDef)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(contextDef);
	GetRHI().SetName(name.Get());
}

void Context::BuildDescriptorSets(const lib::DynamicArray<DynamicDescriptorSetInfo>& setInfos)
{
	m_dynamicDescriptorSets.BuildDescriptorSets(*this, setInfos);
}

rhi::RHIDescriptorSet Context::GetDescriptorSet(DSStateID id) const
{
	return m_dynamicDescriptorSets.GetDescriptorSet(id);
}

} // spt::rdr
