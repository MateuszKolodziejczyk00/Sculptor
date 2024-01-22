#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RHI/RHIBridge/RHILimitsImpl.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "RendererSettings.h"
#include "Renderer.h"
#include "ShaderStructs/ShaderStructsTypes.h"


namespace spt::gfx
{

namespace priv
{

template<typename TStruct, Bool isDynamicOffset>
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

		if constexpr (isDynamicOffset)
		{
			m_offset = owningState.AddDynamicOffset();
			*std::get<DynamicOffset>(m_offset) = 0u;
		}
		else
		{
			m_offset = StaticOffset(0u);
		}

		// construct default value
		new (&GetImpl()) TStruct();
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		if constexpr (isDynamicOffset)
		{
			context.UpdateBuffer(GetName(), m_buffer->CreateFullView());
		}
		else
		{
			constexpr Uint64 structSize = std::max<Uint64>(sizeof(TStruct), 4u);
			const rdr::BufferView boundBufferView(lib::Ref(m_buffer), GetCurrentOffset(), structSize);
			context.UpdateBuffer(GetName(), boundBufferView);
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return rdr::shader_translator::DefineType<TStruct>() + '\n' +
			BuildBindingVariableCode(lib::String("ConstantBuffer<") + rdr::shader_translator::GetTypeName<TStruct>() + "> " + name, bindingIdx);
	}

	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::None;

		if constexpr (isDynamicOffset)
		{
			lib::AddFlag(flags, smd::EBindingFlags::DynamicOffset);
		}

		return { rdr::ShaderBindingMetaData(flags) };
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

	// Dynamic Offset value ptr
	using DynamicOffset = Uint32*;
	using StaticOffset  = Uint32;

	Bool ShouldSwitchBufferOffsetOnChange() const
	{
		return m_structsStride > 0 && rdr::Renderer::GetCurrentFrameIdx() != m_lastDynamicOffsetChangeFrameIdx;
	}

	void SwitchBufferOffsetIfNecessary()
	{
		if (ShouldSwitchBufferOffsetOnChange())
		{
			SPT_CHECK_MSG(m_structsStride != 0, "Buffer {} is not initialized to be updated!", GetName().ToString());
			Uint32& offset = GetCurrentOffsetRef();
			offset += m_structsStride;
			if (offset >= m_buffer->GetRHI().GetSize())
			{
				offset = 0;
			}
			m_lastDynamicOffsetChangeFrameIdx = rdr::Renderer::GetCurrentFrameIdx();

			// If we don't use dynamic offset, we need to wark descriptor as dirty
			if constexpr (!isDynamicOffset)
			{
				MarkAsDirty();
			}
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
		const Uint32 offset = GetCurrentOffset();
		return *reinterpret_cast<TStruct*>(m_bufferMappedPtr + offset);
	}

	Uint32 GetCurrentOffset() const
	{
		if constexpr (isDynamicOffset)
		{
			const Uint32* offset = std::get<DynamicOffset>(m_offset);
			SPT_CHECK(!!offset);
			return *offset;
		}
		else
		{
			return std::get<StaticOffset>(m_offset);
		}
	}

	Uint32& GetCurrentOffsetRef()
	{
		if constexpr (isDynamicOffset)
		{
			Uint32* offset = std::get<DynamicOffset>(m_offset);
			SPT_CHECK(!!offset);
			return *offset;
		}
		else
		{
			return std::get<StaticOffset>(m_offset);
		}
	}

	void InitResources(rdr::DescriptorSetState& owningState)
	{
		SPT_PROFILER_FUNCTION();

		constexpr Uint64 structSize = std::max<Uint64>(sizeof(TStruct), 4u);

		const Bool isMultiFrame = lib::HasAnyFlag(owningState.GetFlags(), rdr::EDescriptorSetStateFlags::Persistent);
		const Uint32 structsCopiesNum = isMultiFrame ? rdr::RendererSettings::Get().framesInFlight : 1;

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
		allocationInfo.memoryUsage     = rhi::EMemoryUsage::CPUToGPU;
		m_buffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(owningState.GetName().ToString() + '.' + GetName().ToString() + lib::String(".Buffer")),
												  bufferDef, allocationInfo);

		m_bufferMappedPtr = m_buffer->GetRHI().MapPtr();
	}

	lib::SharedPtr<rdr::Buffer> m_buffer;
	Byte*                       m_bufferMappedPtr;

	Uint32 m_structsStride;

	// Store frame idx snapshotted when changing value
	// It's used to not changing offset multiple times during one frame as this could override data used on GPU
	Uint64 m_lastDynamicOffsetChangeFrameIdx;

	std::variant<DynamicOffset, StaticOffset> m_offset;
};

} // priv


template<typename TStruct>
using ConstantBufferBindingDynamic = priv::ConstantBufferBinding<TStruct, true>;

template<typename TStruct>
using ConstantBufferBinding = priv::ConstantBufferBinding<TStruct, false>;

} // spt::gfx