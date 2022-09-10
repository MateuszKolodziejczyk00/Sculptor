#include "DescriptorSetState.h"

namespace spt::rdr
{

namespace utils
{

static DSStateID GenerateID()
{
	static DSStateID current = 1;
	return current++;
}

} // utils

DescriptorSetState::DescriptorSetState()
	: m_state(utils::GenerateID())
	, m_isDirty(false)
{ }

DSStateID DescriptorSetState::GetID() const
{
	return m_state;
}

Bool DescriptorSetState::IsDirty() const
{
	return m_isDirty;
}

void DescriptorSetState::ClearDirtyFlag()
{
	m_isDirty = false;
}

void DescriptorSetState::MarkAsDirty()
{
	m_isDirty = true;
}

} // spt::rdr
