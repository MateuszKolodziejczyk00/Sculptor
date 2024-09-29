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

enum class EConstantBufferBindingType
{
	// updating data will create new buffer
	Static,
	// updating data will change static offset and mark descriptor as dirty
	StaticOffset,
	// updating data will only change dynamic offset
	DynamicOffset
};

template<typename TStruct, EConstantBufferBindingType type>
class ConstantBufferBinding : public rdr::DescriptorSetBinding 
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit ConstantBufferBinding(const lib::HashedString& name)
		: Super(name)
		, m_bufferMappedPtr(nullptr)
		, m_structsStride(0)
		, m_lastDynamicOffsetChangeFrameIdx(idxNone<Uint64>)
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

		if constexpr (type == EConstantBufferBindingType::DynamicOffset)
		{
			m_offset = owningState.AddDynamicOffset();
			*std::get<DynamicOffset>(m_offset) = 0u;
		}
		else
		{
			m_offset = StaticOffset(0u);
		}

		if (m_bufferMappedPtr)
		{
			// construct default value
			new (m_bufferMappedPtr + GetCurrentOffset()) TStruct();
		}
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		constexpr Uint64 structSize = GetStructSize();
		const Uint64 offset         = GetStaticOffset();
		const rdr::BufferView boundBufferView(lib::Ref(m_buffer), offset, structSize);
		context.UpdateBuffer(GetBaseBindingIdx(), boundBufferView);
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return rdr::shader_translator::DefineType<TStruct>() + '\n' +
			BuildBindingVariableCode(lib::String("ConstantBuffer<") + rdr::shader_translator::GetTypeName<TStruct>() + "> " + name, bindingIdx);
	}

	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::None;

		constexpr Bool isDynamicOffset = (type == EConstantBufferBindingType::DynamicOffset);
		if constexpr (isDynamicOffset)
		{
			lib::AddFlag(flags, smd::EBindingFlags::DynamicOffset);
		}

		return { rdr::ShaderBindingMetaData(isDynamicOffset ? rhi::EDescriptorType::UniformBufferDynamicOffset : rhi::EDescriptorType::UniformBuffer, flags) };
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
		SwitchBufferMemoryIfNecessary();
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

	static constexpr Bool hasVariableOffset = (type == EConstantBufferBindingType::DynamicOffset || type == EConstantBufferBindingType::StaticOffset);

	static constexpr Uint64 GetStructSize()
	{
		return std::max<Uint64>(sizeof(TStruct), 4u);
	}

	Bool ShouldSwitchBufferOffsetOnChange() const
	{
		return rdr::Renderer::GetCurrentFrameIdx() != m_lastDynamicOffsetChangeFrameIdx;
	}

	void SwitchBufferMemoryIfNecessary()
	{
		if (ShouldSwitchBufferOffsetOnChange())
		{
			if constexpr (hasVariableOffset)
			{
				SPT_CHECK_MSG(m_structsStride != 0, "Buffer {} is not initialized to be updated!", GetName().ToString());
				Uint32& offset = GetCurrentOffsetRef();
				offset += m_structsStride;
				if (offset >= m_buffer->GetRHI().GetSize())
				{
					offset = 0;
				}
				m_lastDynamicOffsetChangeFrameIdx = rdr::Renderer::GetCurrentFrameIdx();

				// If we don't use dynamic offset, we need to mark descriptor as dirty
				if constexpr (type == EConstantBufferBindingType::StaticOffset)
				{
					MarkAsDirty();
				}
			}
			else
			{
				CreateNewBuffer(GetStructSize());
				SPT_CHECK(!!m_bufferMappedPtr);
				// construct default value
				new (m_bufferMappedPtr + GetCurrentOffset()) TStruct();
			}
		}
	}

	void SwitchBufferOffsetPreservingData()
	{
		const TStruct& oldValueAddress = GetImpl();
		const TStruct oldValueCopy = oldValueAddress;
		SwitchBufferMemoryIfNecessary();
		TStruct& newValue = GetImpl();
		
		// If struct offset was changed and we're using other copy, copy prev instance to new, so that changes to value will be incremental
		if (&newValue != &oldValueAddress)
		{
			memcpy_s(&newValue, GetStructSize(), &oldValueCopy, GetStructSize());
		}
	}
	
	TStruct& GetImpl() const
	{
		SPT_CHECK(!!m_bufferMappedPtr);
		const Uint32 offset = GetCurrentOffset();
		return *reinterpret_cast<TStruct*>(m_bufferMappedPtr + offset);
	}

	Uint32 GetStaticOffset() const
	{
		if constexpr (type == EConstantBufferBindingType::StaticOffset)
		{
			return std::get<StaticOffset>(m_offset);
		}
		else
		{
			return 0u;
		}
	}

	Uint32 GetCurrentOffset() const
	{
		if constexpr (type == EConstantBufferBindingType::DynamicOffset)
		{
			const Uint32* offset = std::get<DynamicOffset>(m_offset);
			SPT_CHECK(!!offset);
			return *offset;
		}
		else if constexpr (type == EConstantBufferBindingType::StaticOffset)
		{
			return std::get<StaticOffset>(m_offset);
		}
		else
		{
			return 0u;
		}
	}

	Uint32& GetCurrentOffsetRef()
	{
		SPT_CHECK(hasVariableOffset);

		if constexpr (type == EConstantBufferBindingType::DynamicOffset)
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
		m_name = RENDERER_RESOURCE_NAME(owningState.GetName().ToString() + '.' + GetName().ToString() + lib::String(".Buffer"));

		if constexpr (hasVariableOffset)
		{
			constexpr Uint64 structSize = GetStructSize();

			const Uint64 minOffsetAlignment = rhi::RHILimits::GetMinUniformBufferOffsetAlignment();
			const Uint64 structStride = math::Utils::RoundUp(structSize, minOffsetAlignment);

			const Uint32 structsCopiesNum = rdr::RendererSettings::Get().framesInFlight;

			const Uint64 bufferSize = (structsCopiesNum - 1) * structStride + structSize;

			SPT_CHECK(structStride <= maxValue<Uint32>);
			m_structsStride = static_cast<Uint32>(structStride);

			CreateNewBuffer(bufferSize);
		}
	}

	void CreateNewBuffer(Uint64 size)
	{
		const rhi::BufferDefinition bufferDef(size, rhi::EBufferUsage::Uniform);

		rhi::RHIAllocationInfo allocationInfo;
		allocationInfo.allocationFlags = rhi::EAllocationFlags::CreateMapped;
		allocationInfo.memoryUsage     = rhi::EMemoryUsage::CPUToGPU;
		m_buffer = rdr::ResourcesManager::CreateBuffer(m_name, bufferDef, allocationInfo);

		m_bufferMappedPtr = m_buffer->GetRHI().MapPtr();
	}

	lib::SharedPtr<rdr::Buffer> m_buffer;
	Byte*                       m_bufferMappedPtr;

	rdr::RendererResourceName   m_name;

	Uint32 m_structsStride;

	// Store frame idx snapshotted when changing value
	// It's used to not changing offset multiple times during one frame as this could override data used on GPU
	Uint64 m_lastDynamicOffsetChangeFrameIdx;

	std::variant<DynamicOffset, StaticOffset> m_offset;
};

} // priv


template<typename TStruct>
using ConstantBufferBindingDynamicOffset = priv::ConstantBufferBinding<TStruct, priv::EConstantBufferBindingType::DynamicOffset>;

template<typename TStruct>
using ConstantBufferBindingStaticOffset = priv::ConstantBufferBinding<TStruct, priv::EConstantBufferBindingType::StaticOffset>;

template<typename TStruct>
using ConstantBufferBinding = priv::ConstantBufferBinding<TStruct, priv::EConstantBufferBindingType::Static>;

} // spt::gfx