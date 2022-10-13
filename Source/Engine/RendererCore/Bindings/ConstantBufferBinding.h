#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RHI/RHIBridge/RHILimitsImpl.h"
#include "MathUtils.h"


namespace spt::rdr
{

template<typename TStruct>
class ConstantBufferBinding : public DescriptorSetBinding 
{
public:

	explicit ConstantBufferBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag)
		: DescriptorSetBinding(name, descriptorDirtyFlag)
		, m_secondStructOffset(0)
		, m_offset(nullptr)
	{ }

	void Initialize(DescriptorSetState& owningState)
	{
		DescriptorSetBinding::Initialize(owningState);

		InitResources(owningState);

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

	template<typename TSetter> requires requires (TSetter& setter) { setter(std::declval<TStruct&>()); }
	void Set(TSetter&& setter)
	{
		const TStruct& oldValue = GetImpl();
		SwitchBufferOffset();
		TStruct& newValue = GetImpl();

		SPT_CHECK(&newValue != &oldValue);

		// transfer memory to new location before calling setter
		memcpy_s(&newValue, sizeof(TStruct), &oldValue, sizeof(TStruct));

		setter(newValue);
	}

	const TStruct& Get() const
	{
		return GetImpl();
	}

private:

	Uint64 SwitchBufferOffset()
	{
		*m_offset = *m_offset > 0 ? 0 : m_secondStructOffset;
		return *m_offset;
	}
	
	TStruct& GetImpl() const
	{
		return *reinterpret_cast<TStruct*>(m_buffer->GetRHI().GetMappedPtr() + *m_offset);
	}

	void InitResources(DescriptorSetState& owningState)
	{
		SPT_PROFILER_FUNCTION();

		constexpr Uint64 structSize = sizeof(TStruct);
		const Uint64 minOffsetAlignment = rhi::RHILimits::GetMinUniformBufferOffsetAlignment();
		const Uint64 secondStructOffset = math::Utils::RoundUp(structSize, minOffsetAlignment);
		
		const Uint64 bufferSize = secondStructOffset + structSize;

		rhi::RHIAllocationInfo allocationInfo;
		allocationInfo.allocationFlags = rhi::EAllocationFlags::CreateMapped;
		allocationInfo.memoryUsage = rhi::EMemoryUsage::CPUToGPU;
		m_buffer = ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(owningState.GetName().ToString() + '.' + GetName().ToString() + lib::String(".Buffer")), bufferSize, rhi::EBufferUsage::Uniform, allocationInfo);

		m_bufferView = m_buffer->CreateView(0, bufferSize);

		// store offset as 32bits integer because that's how dynamic offset are stored
		SPT_CHECK(secondStructOffset <= maxValue<Uint32>);
		m_secondStructOffset = static_cast<Uint32>(secondStructOffset);
	}

	lib::SharedPtr<Buffer>		m_buffer;
	lib::SharedPtr<BufferView>	m_bufferView;
	Uint32						m_secondStructOffset;

	// Dynamic Offset value ptr
	Uint32*						m_offset;
};

} // spt::rdr