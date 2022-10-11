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
		, m_offset(nullptr)
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

		// construct default value
		new (&GetImpl()) TStruct();
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

	template<typename TAssignable> requires std::is_assignable_v<TStruct, TAssignable>
	void Set(TAssignable&& value)
	{
		SwitchBufferOffset();
		GetImpl() = std::forward<TAssignable>(value);
	}

	template<typename TSetter> requires requires (TSetter& setter) { setter(std::declval<TStruct>()); }
	void Set(TSetter&& setter)
	{
		const TStruct& oldValue = GetImpl();
		SwitchBufferOffset();
		TStruct& newValue = GetImpl();

		SPT_CHECK(&newValue != oldValue);

		// transfer memory to new location before calling setter
		memcpy_s(&newValue, sizeof(TStruct), &oldValue, sizeof(TStruct));

		setter(newValue);
	}

	const TStruct& Get() const
	{
		return GetImpl();
	}

private:

	Uint32 SwitchBufferOffset()
	{
		*m_offset = *m_offset > 0 ? 0 : sizeof(TStruct);
		return *m_offset;
	}
	
	TStruct& GetImpl() const
	{
		return *reinterpret_cast<TStruct*>(m_buffer->GetRHI().GetMappedPtr() + *m_offset);
	}

	lib::SharedPtr<Buffer>		m_buffer;
	lib::SharedPtr<BufferView>	m_bufferView;

	Uint32*						m_offset;
};

} // spt::rdr