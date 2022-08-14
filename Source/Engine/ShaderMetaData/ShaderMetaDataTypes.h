#pragma once

#include "SculptorCoreTypes.h"

#include <variant>


namespace spt::smd
{

// should be in the same order as BindingDataVariant
namespace EBindingType
{

enum Type : Uint32
{
	Texture,
	CombinedTextureSampler,
	UniformBuffer,
	StorageBuffer,

	NUM
};

} // EBindingType


namespace EBindingFlags
{

enum Flags : Flags32
{
	None			= 0,
	Invalid			= BIT(0),

	Unbound			= BIT(1),
};

}

namespace priv
{

constexpr Flags32 defaultBindingFlags = EBindingFlags::Invalid;

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// BindingData ===================================================================================

struct CommonBindingData
{
	CommonBindingData(Uint32 elementsNum, Flags32 flags)
		: m_elementsNum(elementsNum)
		, m_flags(flags)
	{ }

	void MakeValid()
	{
		lib::RemoveFlag(m_flags, EBindingFlags::Invalid);
	}

	Bool IsValid() const
	{
		return !lib::HasFlag(m_flags, EBindingFlags::Invalid);
	}

	Uint32			m_elementsNum;
	Uint32			m_flags;
};


struct TextureBindingData : public CommonBindingData
{
	TextureBindingData(Uint32 elementsNum = 1, Flags32 flags = priv::defaultBindingFlags)
		: CommonBindingData(elementsNum, flags)
	{ }
};


struct CombinedTextureSamplerBindingData : public CommonBindingData
{
	CombinedTextureSamplerBindingData(Uint32 elementsNum = 1, Flags32 flags = priv::defaultBindingFlags)
		: CommonBindingData(elementsNum, flags)
	{ }
};


struct UniformBufferBindingData : public CommonBindingData
{
	UniformBufferBindingData(Uint32 elementsNum = 1, Flags32 flags = priv::defaultBindingFlags, Uint16 size = 0)
		: CommonBindingData(elementsNum, flags)
		, m_size(size)
	{ }

	Uint16			m_size;
};


struct StorageBufferBindingData : public CommonBindingData
{
	StorageBufferBindingData(Uint32 elementsNum = 1, Flags32 flags = priv::defaultBindingFlags)
		: CommonBindingData(elementsNum, flags)
		, m_size(0) // invalid
	{ }

	void SetSize(Uint16 size)
	{
		SPT_CHECK(IsValid());

		lib::RemoveFlag(m_flags, EBindingFlags::Unbound);
		m_size = size;
	}

	void SetStride(Uint16 stride)
	{
		SPT_CHECK(IsValid());

		lib::AddFlag(m_flags, EBindingFlags::Unbound);
		m_stride = stride;
	}

	Bool IsUnbound() const
	{
		return lib::HasFlag(m_flags, EBindingFlags::Unbound);
	}

	Uint16 GetSize() const
	{
		SPT_CHECK(!IsUnbound());
		return m_size;
	}

	Uint16 GetStride() const
	{
		SPT_CHECK(IsUnbound());
		return m_stride;
	}

private:

	union
	{
		Uint16			m_size;
		Uint16			m_stride;
	};
};


// should be in the same order as EBindingType
using BindingDataVariant = std::variant<TextureBindingData,
										CombinedTextureSamplerBindingData,
										UniformBufferBindingData,
										StorageBufferBindingData>;


struct GenericShaderBinding
{
	GenericShaderBinding()
	{ }

	template<typename TBindingDataType>
	GenericShaderBinding(TBindingDataType bindingData)
		: m_data(bindingData)
	{ }

	template<typename TBindingDataType>
	void Set(TextureBindingData data)
	{
		m_data = data;
	}

	template<typename TBindingDataType>
	TextureBindingData& As()
	{
		return m_data.get<TextureBindingData>();
	}

	template<typename TBindingDataType>
	const TextureBindingData& As() const
	{
		return m_data.get<TextureBindingData>();
	}

	Bool IsValid() const
	{
		return std::visit(
			[](const auto data)
			{
				return data.IsValid();
			},
			m_data);
	}

	EBindingType::Type GetBindingType() const
	{
		return static_cast<EBindingType::Type>(m_data.index());
	}

private:

	BindingDataVariant			m_data;
};


struct ShaderDescriptorSet
{
public:

	ShaderDescriptorSet() = default;

	template<typename TBindingDataType>
	void AddBinding(Uint8 bindingIdx, TextureBindingData bindingData)
	{
		const SizeType newBindingIdx = static_cast<SizeType>(bindingIdx);

		if (bindingIdx >= m_bindings.size())
		{
			m_bindings.resize(newBindingIdx + 1);
		}

		SPT_CHECK(!m_bindings[newBindingIdx].IsValid());

		m_bindings[newBindingIdx].Set(bindingData);
	}

	const lib::DynamicArray<GenericShaderBinding>& GetBindings() const
	{
		return m_bindings;
	}

private:

	lib::DynamicArray<GenericShaderBinding>		m_bindings;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderParamEntry ==============================================================================

struct ShaderParamEntryCommon
{
	ShaderParamEntryCommon(Uint8 setIdx, Uint8 bindingIdx)
		: m_setIdx(setIdx)
		, m_bindingIdx(bindingIdx)
	{ }

	Uint8		m_setIdx;
	Uint8		m_bindingIdx;
};


struct ShaderTextureParamEntry : public ShaderParamEntryCommon
{
	ShaderTextureParamEntry(Uint8 setIdx, Uint8 bindingIdx)
		: ShaderParamEntryCommon(setIdx, bindingIdx)
	{ }

	// No additional data for now
};


struct ShaderDataParamEntry : public ShaderParamEntryCommon
{
	ShaderDataParamEntry(Uint8 setIdx, Uint8 bindingIdx, Uint16 offset, Uint16 size)
		: ShaderParamEntryCommon(setIdx, bindingIdx)
		, m_offset(offset)
		, m_size(size)
	{ }

	Uint16		m_offset;
	Uint16		m_size;
};


using ShaderParamEntryVariant = std::variant<ShaderTextureParamEntry,
											 ShaderDataParamEntry>;


struct GenericShaderParamEntry
{
public:

	template<typename TEntryDataType>
	GenericShaderParamEntry(TEntryDataType entry)
		: m_data(entry)
	{ }

	template<typename TEntryDataType>
	const TEntryDataType& As() const
	{
		return m_data.get<TEntryDataType>();
	}

	template<typename TEntryDataType>
	TEntryDataType GetOrDefault() const
	{
		const TEntryDataType* foundData = m_data.get_if<TEntryDataType>();
		return foundData != nullptr ? *foundData : TEntryDataType();
	}

private:

	const ShaderParamEntryVariant		m_data;
};

} // spt::smd
