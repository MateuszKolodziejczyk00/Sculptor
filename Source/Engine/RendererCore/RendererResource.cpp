#include "RendererResource.h"
#include "Utility/Templates/Overload.h"
#include "Types/GPUMemoryPool.h"

#include <variant>


namespace spt::rdr
{

AllocationDefinition::AllocationDefinition(const CommitedAllocationDef& allocationDef)
	: m_allocationDef(allocationDef)
{ }

AllocationDefinition::AllocationDefinition(rhi::EMemoryUsage memoryUsage)
	: AllocationDefinition(CommitedAllocationDef{ memoryUsage })
{ }

AllocationDefinition::AllocationDefinition(const rhi::RHIAllocationInfo& allocationInfo)
	: AllocationDefinition(CommitedAllocationDef{ allocationInfo })
{ }

AllocationDefinition::AllocationDefinition(const PlacedAllocationDef& allocationDef)
	: m_allocationDef(allocationDef)
{ }

Bool AllocationDefinition::IsCommited() const
{
	return std::holds_alternative<CommitedAllocationDef>(m_allocationDef);
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
									  [](const CommitedAllocationDef& def) -> rhi::RHIResourceAllocationDefinition
									  {
										  return rhi::RHICommitedAllocationDefinition(def.allocationInfo);
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

const CommitedAllocationDef& AllocationDefinition::GetCommitedAllocationDef() const
{
	SPT_CHECK(IsCommited());
	return std::get<CommitedAllocationDef>(m_allocationDef);
}

} // spt::rdr