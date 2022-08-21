#pragma once

#include "SculptorCoreTypes.h"
#include "RHI/RHICore/RHIShaderTypes.h"

#include <variant>


namespace spt::smd
{

// should be in the same order as BindingDataVariant
enum class EBindingType : Uint32
{
	Texture,
	CombinedTextureSampler,
	UniformBuffer,
	StorageBuffer,

	NUM
};


enum class EBindingFlags : Flags32
{
	None			= 0,
	Invalid			= BIT(0),

	Unbound			= BIT(1),

	// Shader stages
	VertexShader	= BIT(21),
	FragmentShader	= BIT(22),
	ComputeShader	= BIT(23),
};


namespace priv
{

constexpr EBindingFlags defaultBindingFlags = EBindingFlags::Invalid;

inline rhi::EShaderStageFlags BindingFlagsToShaderStageFlags(EBindingFlags bindingFlags)
{
	rhi::EShaderStageFlags shaderStageFlags = rhi::EShaderStageFlags::None;

	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::VertexShader))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::Vertex);
	}
	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::FragmentShader))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::Fragment);
	}
	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::ComputeShader))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::Compute);
	}

	return shaderStageFlags;
}

inline EBindingFlags ShaderStageToBindingFlag(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::Vertex:		return EBindingFlags::VertexShader;
	case rhi::EShaderStage::Fragment:	return EBindingFlags::FragmentShader;
	case rhi::EShaderStage::Compute:	return EBindingFlags::ComputeShader;
	}

	return EBindingFlags::None;
}

} // priv

static constexpr Uint32 maxSetIdx		= 255;
static constexpr Uint32 maxBindingIdx	= 255;

//////////////////////////////////////////////////////////////////////////////////////////////////
// BindingData ===================================================================================

struct CommonBindingData abstract
{
	explicit CommonBindingData(Uint32 inElementsNum, EBindingFlags inFlags)
		: elementsNum(inElementsNum)
		, flags(inFlags)
	{ }

	void MakeValid()
	{
		lib::RemoveFlag(flags, EBindingFlags::Invalid);
	}

	Bool IsValid() const
	{
		return !lib::HasAnyFlag(flags, EBindingFlags::Invalid);
	}

	void AddShaderStage(rhi::EShaderStage stage)
	{
		lib::AddFlag(flags, priv::ShaderStageToBindingFlag(stage));
	}

	rhi::EShaderStageFlags GetShaderStages() const
	{
		return priv::BindingFlagsToShaderStageFlags(flags);
	}

	Uint32			elementsNum;
	EBindingFlags	flags;
};


struct TextureBindingData : public CommonBindingData
{
	explicit TextureBindingData(Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
	{ }
};


struct CombinedTextureSamplerBindingData : public CommonBindingData
{
	explicit CombinedTextureSamplerBindingData(Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
	{ }
};


struct UniformBufferBindingData : public CommonBindingData
{
	explicit UniformBufferBindingData(Uint16 inSize = 0, Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
		, size(inSize)
	{
		SPT_CHECK(size > 0);
	}

	Uint16			size;
};


struct StorageBufferBindingData : public CommonBindingData
{
	explicit StorageBufferBindingData(Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
		, m_size(0) // invalid
	{ }

	void SetSize(Uint16 size)
	{
		lib::RemoveFlag(flags, EBindingFlags::Unbound);
		m_size = size;
	}

	void SetUnbound()
	{
		lib::AddFlag(flags, EBindingFlags::Unbound);
	}

	Bool IsUnbound() const
	{
		return lib::HasAnyFlag(flags, EBindingFlags::Unbound);
	}

	Uint16 GetSize() const
	{
		SPT_CHECK(!IsUnbound());
		return m_size;
	}

private:

	Uint16			m_size;
};


// should be in the same order as EBindingType
using BindingDataVariant = std::variant<TextureBindingData,
										CombinedTextureSamplerBindingData,
										UniformBufferBindingData,
										StorageBufferBindingData>;


struct GenericShaderBinding
{
	GenericShaderBinding() = default;

	template<typename TBindingDataType>
	explicit GenericShaderBinding(TBindingDataType bindingData)
		: m_data(bindingData)
	{ }

	template<typename TBindingDataType>
	void Set(TBindingDataType data)
	{
		m_data = data;
	}

	template<typename TBindingDataType>
	TBindingDataType& As()
	{
		return std::get<TBindingDataType>(m_data);
	}

	template<typename TBindingDataType>
	const TBindingDataType& As() const
	{
		return std::get<TBindingDataType>(m_data);
	}

	template<typename TBindingDataType>
	Bool Contains() const
	{
		return std::holds_alternative<TBindingDataType>(m_data);
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

	EBindingType GetBindingType() const
	{
		return static_cast<EBindingType>(m_data.index());
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
	void AddBinding(Uint8 bindingIdx, TBindingDataType bindingData)
	{
		const SizeType newBindingIdx = static_cast<SizeType>(bindingIdx);

		if (bindingIdx >= m_bindings.size())
		{
			m_bindings.resize(newBindingIdx + 1);
		}

		if (!(m_bindings[newBindingIdx].IsValid()))
		{
			// make sure that this binding will be marked as valid
			bindingData.MakeValid();

			m_bindings[newBindingIdx].Set(bindingData);
		}
		else
		{
			SPT_CHECK(m_bindings[newBindingIdx].Contains<TBindingDataType>());
			SPT_CHECK(memcmp(&m_bindings[newBindingIdx].As<TBindingDataType>(), &bindingData, sizeof(TBindingDataType)));
		}

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
	ShaderParamEntryCommon() = default;

	ShaderParamEntryCommon(Uint8 inSetIdx, Uint8 inBindingIdx)
		: setIdx(inSetIdx)
		, bindingIdx(inBindingIdx)
	{ }

	Uint8		setIdx;
	Uint8		bindingIdx;
};


struct ShaderTextureParamEntry : public ShaderParamEntryCommon
{
	ShaderTextureParamEntry() = default;

	ShaderTextureParamEntry(Uint8 inSetIdx, Uint8 inBindingIdx)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
	{ }

	// No additional data for now
};


struct ShaderCombinedTextureSamplerParamEntry : public ShaderParamEntryCommon
{
	ShaderCombinedTextureSamplerParamEntry() = default;

	ShaderCombinedTextureSamplerParamEntry(Uint8 inSetIdx, Uint8 inBindingIdx)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
	{ }

	// No additional data for now
};


struct ShaderDataParamEntry : public ShaderParamEntryCommon
{
	ShaderDataParamEntry() = default;

	ShaderDataParamEntry(Uint8 inSetIdx, Uint8 inBindingIdx, Uint16 inOffset, Uint16 inSize, Uint16 inStride)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
		, offset(inOffset)
		, size(inSize)
		, stride(inStride)
	{ }

	Uint16		offset;
	Uint16		size;
	Uint16		stride;
};


struct ShaderBufferParamEntry : public ShaderParamEntryCommon
{
	ShaderBufferParamEntry() = default;

	explicit ShaderBufferParamEntry(Uint8 inSetIdx, Uint8 inBindingIdx)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
	{ }

	// No additional data for now
};


using ShaderParamEntryVariant = std::variant<ShaderTextureParamEntry,
											 ShaderCombinedTextureSamplerParamEntry,
											 ShaderDataParamEntry,
											 ShaderBufferParamEntry>;


struct GenericShaderParamEntry
{
public:

	GenericShaderParamEntry() = default;

	template<typename TEntryDataType>
	explicit GenericShaderParamEntry(TEntryDataType entry)
		: m_data(entry)
	{ }

	GenericShaderParamEntry& operator=(const GenericShaderParamEntry& rhs)
	{
		m_data = rhs.m_data;

		return *this;
	}

	template<typename TEntryDataType>
	void Set(TEntryDataType entry)
	{
		m_data = entry;
	}

	template<typename TEntryDataType>
	const TEntryDataType& As() const
	{
		return std::get<TEntryDataType>(m_data);
	}

	template<typename TEntryDataType>
	Bool Contains() const
	{
		return std::holds_alternative<TEntryDataType>(m_data);
	}

	template<typename TEntryDataType>
	TEntryDataType GetOrDefault() const
	{
		const TEntryDataType* foundData = std::get_if<TEntryDataType>(m_data);
		return foundData != nullptr ? *foundData : TEntryDataType();
	}

	ShaderParamEntryCommon GetCommonData() const
	{
		return std::visit(
			[](const auto data) -> ShaderParamEntryCommon
			{
				return ShaderParamEntryCommon(data.setIdx, data.bindingIdx);
			},
			m_data);
	}

private:

	ShaderParamEntryVariant		m_data;
};

} // spt::smd
