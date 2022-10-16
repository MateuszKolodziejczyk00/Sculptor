#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetStateTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
#include "DynamicDescriptorSetsTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"


namespace spt::rdr
{

class Context;


class DynamicDescriptorSetsManager
{
public:

	DynamicDescriptorSetsManager();

	void BuildDescriptorSets(Context& renderContext, const lib::DynamicArray<DynamicDescriptorSetInfo>& dsInfos);

	rhi::RHIDescriptorSet GetDescriptorSet(DSStateID id) const;

private:

	lib::HashMap<DSStateID, rhi::RHIDescriptorSet> m_descriptorSets;
};

} // spt::rdr