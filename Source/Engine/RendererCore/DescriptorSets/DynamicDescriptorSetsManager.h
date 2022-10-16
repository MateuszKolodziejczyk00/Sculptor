#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetStateTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
#include "DynamicDescriptorSetsTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"


namespace spt::rdr
{

class RenderContext;


class DynamicDescriptorSetsManager
{
public:

	DynamicDescriptorSetsManager();

	void BuildDescriptorSets(RenderContext& renderContext, const lib::DynamicArray<DynamicDescriptorSetInfo>& dsInfos);

	rhi::RHIDescriptorSet GetDescriptorSet(DSStateID id) const;

private:

	lib::HashMap<DSStateID, rhi::RHIDescriptorSet> m_descriptorSets;
};

} // spt::rdr