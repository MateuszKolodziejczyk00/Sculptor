#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/AccelerationStructure.h"


namespace spt::rdr
{
class TopLevelAS;
} // spt::rdr


namespace spt::gfx
{

class AccelerationStructureBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit AccelerationStructureBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateAccelerationStructure(GetBaseBindingIdx(), lib::Ref(m_tlas));
	}

	AccelerationStructureBinding& operator=(const lib::SharedRef<rdr::TopLevelAS>& tlas)
	{
		if (m_tlas != tlas.ToSharedPtr())
		{
			m_tlas = tlas;
			MarkAsDirty();
		}
		return *this;
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return BuildBindingVariableCode(lib::String("RaytracingAccelerationStructure ") + name, bindingIdx);
	}

	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::AccelerationStructure) };
	}

private:

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;
};

} // spt::gfx