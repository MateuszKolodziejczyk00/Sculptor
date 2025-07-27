#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/AccelerationStructure.h"
#include "Types/Buffer.h"


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

	virtual void UpdateDescriptors(rdr::DescriptorSetIndexer& indexer) const final
	{
		const lib::SharedPtr<rdr::Buffer> tlasBuffer = m_tlas->GetTLASDataBuffer();
		tlasBuffer->GetRHI().CopyTLASDescriptor(indexer[GetBaseBindingIdx()][0]);
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