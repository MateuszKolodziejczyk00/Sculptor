#pragma once

#include "SculptorCoreTypes.h"
#include "RHI/RHICore/RHIShaderTypes.h"

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

	// Shader stages
	VertexShader	= BIT(21),
	FragmentShader	= BIT(22),
	ComputeShader	= BIT(23),
};

} // EBindingFlags


namespace priv
{

constexpr Flags32 defaultBindingFlags = EBindingFlags::Invalid;

inline Flags32 BindingFlagsToShaderStageFlags(Flags32 bindingFlags)
{
	Flags32 shaderStageFlags = 0;

	if (lib::HasFlag(bindingFlags, EBindingFlags::VertexShader))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::Vertex);
	}
	if (lib::HasFlag(bindingFlags, EBindingFlags::FragmentShader))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::Fragment);
	}
	if (lib::HasFlag(bindingFlags, EBindingFlags::ComputeShader))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::Compute);
	}

	return shaderStageFlags;
}

inline EBindingFlags::Flags ShaderStageToBindingFlag(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::Vertex:		return EBindingFlags::VertexShader;
	case rhi::EShaderStage::Fragment:	return EBindingFlags::FragmentShader;
	case rhi::EShaderStage::Compute:	return EBindingFlags::ComputeShader;
	}

	return EBindingFlags::None;
}

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

	void AddShaderStage(rhi::EShaderStage stage)
	{
		lib::AddFlag(m_flags, priv::ShaderStageToBindingFlag(stage));
	}

	Flags32 GetShaderStages() const
	{
		return priv::BindingFlagsToShaderStageFlags(m_flags);
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

	void AddShaderStage(rhi::EShaderStage stage)
	{
		return std::visit(
			[stage](auto& data)
			{
				return data.AddShaderStage(stage);
			},
			m_data);
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

		// make sure that this binding will be marked as valid
		bindingData.MakeValid();

		m_bindings[newBindingIdx].Set(bindingData);
	}

	lib::DynamicArray<GenericShaderBinding>& GetBindings()
	{
		return m_bindings;
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
