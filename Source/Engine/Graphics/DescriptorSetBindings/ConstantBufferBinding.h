#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RHI/RHIBridge/RHILimitsImpl.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "RendererSettings.h"
#include "Renderer.h"


namespace spt::gfx
{

namespace priv
{

template<typename TStruct, Bool isImmutable>
class ConstantBufferBinding : public rdr::DescriptorSetBinding 
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit ConstantBufferBinding(const lib::HashedString& name)
		: Super(name)
		, m_bufferMappedPtr(nullptr)
		, m_structsStride(0)
		, m_lastDynamicOffsetChangeFrameIdx(rdr::Renderer::GetCurrentFrameIdx())
		, m_offset(nullptr)
	{ }

	~ConstantBufferBinding()
	{
		if (m_bufferMappedPtr)
		{
			SPT_CHECK(!!m_buffer);
		
			m_buffer->GetRHI().Unmap();
			m_bufferMappedPtr = nullptr;
		}
	}

	void Initialize(rdr::DescriptorSetState& owningState)
	{
		Super::Initialize(owningState);

		InitResources(owningState);

		if constexpr (!isImmutable)
		{
			m_offset = owningState.AddDynamicOffset();
			*m_offset = 0;
		}

		// construct default value
		new (&GetImpl()) TStruct();
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateBuffer(GetName(), m_buffer->CreateFullView());
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return rdr::shader_translator::DefineType<TStruct>() + '\n' +
			BuildBindingVariableCode(lib::String("ConstantBuffer<") + rdr::shader_translator::GetTypeName<TStruct>() + "> " + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::None;

		if (!isImmutable)
		{
			lib::AddFlag(flags, smd::EBindingFlags::DynamicOffset);
		}

		return flags;
	}

	template<typename TAssignable> requires std::is_assignable_v<TStruct, TAssignable>
	ConstantBufferBinding& operator=(TAssignable&& assignable)
	{
		Set(std::forward<TAssignable>(assignable));
		return *this;
	}

	template<typename TAssignable> requires std::is_assignable_v<TStruct, TAssignable>
	void Set(TAssignable&& value)
	{
		SwitchBufferOffsetIfNecessary();
		GetImpl() = std::forward<TAssignable>(value);
	}

	template<typename TSetter> requires requires (TSetter& setter) { setter(std::declval<TStruct&>()); }
	void Set(TSetter&& setter)
	{
		SwitchBufferOffsetPreservingData();
		setter(GetImpl());
	}

	TStruct& GetMutable()
	{
		SwitchBufferOffsetPreservingData();
		return GetImpl();
	}

	const TStruct& Get() const
	{
		return GetImpl();
	}

private:

	Bool ShouldSwitchBufferOffsetOnChange() const
	{
		return m_structsStride > 0 && rdr::Renderer::GetCurrentFrameIdx() != m_lastDynamicOffsetChangeFrameIdx;
	}

	Uint64 SwitchBufferOffsetIfNecessary()
	{
		if constexpr (isImmutable)
		{
			SPT_CHECK_MSG(rdr::Renderer::GetCurrentFrameIdx() == m_lastDynamicOffsetChangeFrameIdx, "Immutable constant buffers can be changed only during same frame when they were created");
			return 0;
		}
		else
		{
			SPT_CHECK(!!m_offset);
			if (ShouldSwitchBufferOffsetOnChange())
			{
				SPT_CHECK(m_structsStride != 0);
				*m_offset += m_structsStride;
				if (*m_offset >= m_buffer->GetRHI().GetSize())
				{
					*m_offset = 0;
				}
				m_lastDynamicOffsetChangeFrameIdx = rdr::Renderer::GetCurrentFrameIdx();
			}
			return *m_offset;
		}
	}

	void SwitchBufferOffsetPreservingData()
	{
		const TStruct& oldValue = GetImpl();
		SwitchBufferOffsetIfNecessary();
		TStruct& newValue = GetImpl();
		
		// If struct offset was changed and we're using other copy, copy prev instance to new, so that changes to value will be incremental
		if (&newValue != &oldValue)
		{
			memcpy_s(&newValue, sizeof(TStruct), &oldValue, sizeof(TStruct));
		}
	}
	
	TStruct& GetImpl() const
	{
		SPT_CHECK(!!m_bufferMappedPtr);
		const Uint64 offset = m_offset ? *m_offset : 0;
		return *reinterpret_cast<TStruct*>(m_bufferMappedPtr + offset);
	}

	void InitResources(rdr::DescriptorSetState& owningState)
	{
		SPT_PROFILER_FUNCTION();

		constexpr Uint64 structSize = std::max<Uint64>(sizeof(TStruct), 4u);

		const Bool isPersistent = lib::HasAnyFlag(owningState.GetFlags(), rdr::EDescriptorSetStateFlags::Persistent) || isImmutable;
		const Uint32 structsCopiesNum = isPersistent ? rdr::RendererSettings::Get().framesInFlight : 1;

		Uint64 bufferSize = 0;
		if (structsCopiesNum > 1)
		{
			const Uint64 minOffsetAlignment = rhi::RHILimits::GetMinUniformBufferOffsetAlignment();
			const Uint64 structStride = math::Utils::RoundUp(structSize, minOffsetAlignment);

			bufferSize = (structsCopiesNum - 1) * structStride + structSize;

			SPT_CHECK(structStride <= maxValue<Uint32>);
			m_structsStride = static_cast<Uint32>(structStride);
		}
		else
		{
			bufferSize = structSize;
		}

		const rhi::BufferDefinition bufferDef(bufferSize, rhi::EBufferUsage::Uniform);

		rhi::RHIAllocationInfo allocationInfo;
		allocationInfo.allocationFlags = rhi::EAllocationFlags::CreateMapped;
		allocationInfo.memoryUsage = rhi::EMemoryUsage::CPUToGPU;
		m_buffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(owningState.GetName().ToString() + '.' + GetName().ToString() + lib::String(".Buffer")),
												  bufferDef, allocationInfo);

		m_bufferMappedPtr = m_buffer->GetRHI().MapPtr();
	}

	lib::SharedPtr<rdr::Buffer>		m_buffer;
	Byte*							m_bufferMappedPtr;

	Uint32							m_structsStride;

	// Store frame idx snapshotted when changing value
	// It's used to not changing offset multiple times during one frame as this could override data used on GPU
	Uint64							m_lastDynamicOffsetChangeFrameIdx;

	// Dynamic Offset value ptr
	Uint32*							m_offset;
};

} // priv


template<typename TStruct>
using ConstantBufferBinding = priv::ConstantBufferBinding<TStruct, false>;

template<typename TStruct>
using ImmutableConstantBufferBinding = priv::ConstantBufferBinding<TStruct, true>;

} // spt::gfx