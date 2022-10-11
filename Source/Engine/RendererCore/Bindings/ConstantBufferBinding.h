#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"


namespace spt::rdr
{

template<typename TStruct>
class ConstantBufferBinding : public DescriptorSetBinding 
{
public:

	explicit ConstantBufferBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag)
		: DescriptorSetBinding(name, descriptorDirtyFlag)
	{
		rhi::RHIAllocationInfo allocationInfo;
		allocationInfo.allocationFlags = rhi::EAllocationFlags::CreateMapped;
		allocationInfo.memoryUsage = rhi::EMemoryUsage::CPUToGPU;
		m_buffer = ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(GetName().ToString() + lib::String("_Buffer")), 2 * sizeof(TStruct), rhi::EBufferUsage::Uniform, allocationInfo);
		m_bufferView = m_buffer->CreateView(0, m_buffer->GetRHI().GetSize());
	}

	void Initialize(DescriptorSetState& owningState)
	{
		DescriptorSetBinding::Initialize(owningState);

		m_offset = owningState.AddDynamicOffset();
		*m_offset = 0;
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateBuffer(GetName(), lib::Ref(m_bufferView));
	}

	void CreateBindingMetaData(OUT smd::GenericShaderBinding& binding) const
	{
		smd::BufferBindingData bindingData(1, GetBindingFlags());
		bindingData.SetSize(sizeof(TStruct));
		binding.Set(bindingData);
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return lib::String("[[shader_struct(") + TStruct::GetStructName() + ")]]\n" + BuildBindingVariableCode(lib::String("ConstantBuffer<") + TStruct::GetStructName() + "> " + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return smd::EBindingFlags::DynamicOffset;
	}

	void Set(const TStruct& value)
	{
		*m_offset = *m_offset > 0 ? 0 : sizeof(TStruct);
		void* bufferPtr = m_buffer->GetRHI().GetMappedPtr() + m_offset;
		TStruct* structPtr = static_cast<TStruct*>(bufferPtr);
		*structPtr = value;
	}

private:

	lib::SharedPtr<Buffer>		m_buffer;
	lib::SharedPtr<BufferView>	m_bufferView;

	Uint32*						m_offset;
};

} // spt::rdr