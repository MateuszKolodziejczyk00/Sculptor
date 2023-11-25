#include "RendererResource.h"
#include "Utility/Templates/Overload.h"
#include "Types/GPUMemoryPool.h"

#include <variant>


namespace spt::rdr
{

AllocationDefinition::AllocationDefinition()
	: m_allocationDef(NullAllocationDef{})
{ }

AllocationDefinition::AllocationDefinition(const CommittedAllocationDef& allocationDef)
	: m_allocationDef(allocationDef)
{ }

AllocationDefinition::AllocationDefinition(rhi::EMemoryUsage memoryUsage)
	: AllocationDefinition(CommittedAllocationDef{ memoryUsage })
{ }

AllocationDefinition::AllocationDefinition(const rhi::RHIAllocationInfo& allocationInfo)
	: AllocationDefinition(CommittedAllocationDef{ allocationInfo })
{ }

AllocationDefinition::AllocationDefinition(const PlacedAllocationDef& allocationDef)
	: m_allocationDef(allocationDef)
{ }

Bool AllocationDefinition::IsCommitted() const
{
	return std::holds_alternative<CommittedAllocationDef>(m_allocationDef);
}

Bool AllocationDefinition::IsPlaced() const
{
	return std::holds_alternative<PlacedAllocationDef>(m_allocationDef);
}

Bool AllocationDefinition::IsNull() const
{
	return std::holds_alternative<NullAllocationDef>(m_allocationDef);
}

rhi::RHIResourceAllocationDefinition AllocationDefinition::GetRHIAllocationDef() const
{
	rhi::RHIResourceAllocationDefinition rhiAllocationDef;

	rhiAllocationDef = std::visit(lib::Overload
								  {
									  [](const NullAllocationDef& def) -> rhi::RHIResourceAllocationDefinition
									  {
										  return rhi::RHINullAllocationDefinition();
									  },
									  [](const CommittedAllocationDef& def) -> rhi::RHIResourceAllocationDefinition
									  {
										  return rhi::RHICommittedAllocationDefinition(def.allocationInfo);
									  },
									  [](const PlacedAllocationDef& def) -> rhi::RHIResourceAllocationDefinition
									  {
										  return rhi::RHIPlacedAllocationDefinition(&def.memoryPool->GetRHI(), def.allocationFlags);
									  }
								  },
								  m_allocationDef);

	return rhiAllocationDef;
}

const AllocationDefinition::AllocationDefinitionVariant& AllocationDefinition::GetAllocationDef() const
{
	return m_allocationDef;
}

const PlacedAllocationDef& AllocationDefinition::GetPlacedAllocationDef() const
{
	SPT_CHECK(IsPlaced());
	return std::get<PlacedAllocationDef>(m_allocationDef);
}

const CommittedAllocationDef& AllocationDefinition::GetCommittedAllocationDef() const
{
	SPT_CHECK(IsCommitted());
	return std::get<CommittedAllocationDef>(m_allocationDef);
}

} // spt::rdr