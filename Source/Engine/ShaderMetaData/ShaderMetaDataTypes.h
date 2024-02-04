#pragma once

#include "SculptorCoreTypes.h"
#include "RHI/RHICore/RHIShaderTypes.h"
#include "RHI/RHICore/RHIDescriptorTypes.h"
#include "RHI/RHICore/RHISamplerTypes.h"

#include <variant>


namespace spt::smd
{

// should be in the same order as BindingDataVariant
enum class EBindingType : Uint32
{
	Texture,
	CombinedTextureSampler,
	Buffer,
	Sampler,
	AccelerationStructure,

	NUM
};


enum class EBindingFlags : Flags32
{
	None				= 0,
	Invalid				= BIT(0),

	Unbound				= BIT(1),
	DynamicOffset		= BIT(2),
	TexelBuffer			= BIT(3),
	Storage				= BIT(4),
	
	PartiallyBound		= BIT(5),

	ImmutableSampler	= BIT(6),

	// Shader stages
	VertexShader		= BIT(21),
	FragmentShader		= BIT(22),
	ComputeShader		= BIT(23),
	RTGeneration		= BIT(24),
	RTAnyHit			= BIT(25),
	RTClosestHit		= BIT(26),
	RTMiss				= BIT(27),
	RTIntersection		= BIT(28),

	AllShaders			= VertexShader | FragmentShader | ComputeShader | RTGeneration | RTAnyHit | RTClosestHit | RTMiss | RTIntersection
};


namespace priv
{

constexpr EBindingFlags defaultBindingFlags = EBindingFlags::None;

constexpr rhi::EShaderStageFlags BindingFlagsToShaderStageFlags(EBindingFlags bindingFlags)
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
	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::RTGeneration))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::RTGeneration);
	}
	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::RTAnyHit))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::RTAnyHit);
	}
	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::RTClosestHit))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::RTClosestHit);
	}
	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::RTMiss))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::RTMiss);
	}
	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::RTIntersection))
	{
		lib::AddFlag(shaderStageFlags, rhi::EShaderStageFlags::RTIntersection);
	}

	return shaderStageFlags;
}

constexpr EBindingFlags ShaderStageToBindingFlag(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::Vertex:			return EBindingFlags::VertexShader;
	case rhi::EShaderStage::Fragment:		return EBindingFlags::FragmentShader;
	case rhi::EShaderStage::Compute:		return EBindingFlags::ComputeShader;
	case rhi::EShaderStage::RTGeneration:	return EBindingFlags::RTGeneration;
	case rhi::EShaderStage::RTAnyHit:		return EBindingFlags::RTAnyHit;
	case rhi::EShaderStage::RTClosestHit:	return EBindingFlags::RTClosestHit;
	case rhi::EShaderStage::RTMiss:			return EBindingFlags::RTMiss;
	case rhi::EShaderStage::RTIntersection:	return EBindingFlags::RTIntersection;
	}

	return EBindingFlags::None;
}

constexpr EBindingFlags ShaderStageFlagsToBindingFlags(rhi::EShaderStageFlags flags)
{
	EBindingFlags bindingFlags = EBindingFlags::None;

	if (lib::HasAnyFlag(flags, rhi::EShaderStageFlags::Vertex))
	{
		lib::AddFlag(bindingFlags, EBindingFlags::VertexShader);
	}
	if (lib::HasAnyFlag(flags, rhi::EShaderStageFlags::Fragment))
	{
		lib::AddFlag(bindingFlags, EBindingFlags::FragmentShader);
	}
	if (lib::HasAnyFlag(flags, rhi::EShaderStageFlags::Compute))
	{
		lib::AddFlag(bindingFlags, EBindingFlags::ComputeShader);
	}
	if (lib::HasAnyFlag(flags, rhi::EShaderStageFlags::RTGeneration))
	{
		lib::AddFlag(bindingFlags, EBindingFlags::RTGeneration);
	}
	if (lib::HasAnyFlag(flags, rhi::EShaderStageFlags::RTAnyHit))
	{
		lib::AddFlag(bindingFlags, EBindingFlags::RTAnyHit);
	}
	if (lib::HasAnyFlag(flags, rhi::EShaderStageFlags::RTClosestHit))
	{
		lib::AddFlag(bindingFlags, EBindingFlags::RTClosestHit);
	}
	if (lib::HasAnyFlag(flags, rhi::EShaderStageFlags::RTMiss))
	{
		lib::AddFlag(bindingFlags, EBindingFlags::RTMiss);
	}
	if (lib::HasAnyFlag(flags, rhi::EShaderStageFlags::RTIntersection))
	{
		lib::AddFlag(bindingFlags, EBindingFlags::RTIntersection);
	}

	return bindingFlags;
}

constexpr rhi::EDescriptorType SelectBufferDescriptorType(EBindingFlags bindingFlags)
{
	if (lib::HasAnyFlag(bindingFlags, EBindingFlags::Storage))
	{
		if (lib::HasAllFlags(bindingFlags, EBindingFlags::DynamicOffset))
		{
			return rhi::EDescriptorType::StorageBufferDynamicOffset;
		}
		else if (lib::HasAllFlags(bindingFlags, EBindingFlags::TexelBuffer))
		{
			return rhi::EDescriptorType::StorageTexelBuffer;
		}
		else
		{
			return rhi::EDescriptorType::StorageBuffer;
		}
	}
	else // Uniform
	{
		if (lib::HasAllFlags(bindingFlags, EBindingFlags::DynamicOffset))
		{
			return rhi::EDescriptorType::UniformBufferDynamicOffset;
		}
		else if (lib::HasAllFlags(bindingFlags, EBindingFlags::TexelBuffer))
		{
			return rhi::EDescriptorType::UniformTexelBuffer;
		}
		else
		{
			return rhi::EDescriptorType::UniformBuffer;
		}
	}
}

constexpr rhi::EDescriptorType SelectTextureDescriptorType(EBindingFlags bindingFlags)
{
	return lib::HasAnyFlag(bindingFlags, EBindingFlags::Storage)
		? rhi::EDescriptorType::StorageTexture
		: rhi::EDescriptorType::SampledTexture;
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
		, bindingDescriptorType(rhi::EDescriptorType::None)
	{ }

	void MakeInvalid()
	{
		lib::AddFlag(flags, EBindingFlags::Invalid);
	}

	Bool IsValid() const
	{
		return !lib::HasAnyFlag(flags, EBindingFlags::Invalid);
	}

	void AddFlag(EBindingFlags flag)
	{
		lib::AddFlag(flags, flag);
	}

	void AddShaderStage(rhi::EShaderStage stage)
	{
		lib::AddFlag(flags, priv::ShaderStageToBindingFlag(stage));
	}

	void AddShaderStages(rhi::EShaderStageFlags stages)
	{
		lib::AddFlag(flags, priv::ShaderStageFlagsToBindingFlags(stages));
	}

	rhi::EShaderStageFlags GetShaderStages() const
	{
		return priv::BindingFlagsToShaderStageFlags(flags);
	}

	rhi::EDescriptorType GetDescriptorType() const
	{
		return bindingDescriptorType;
	}

	bool TryMergeWithOtherBinding(const CommonBindingData& other)
	{
		if (bindingDescriptorType == other.bindingDescriptorType && elementsNum == other.elementsNum)
		{
			lib::AddFlag(flags, other.flags);
			return true;
		}

		return false;
	}

	SizeType Hash() const
	{
		return lib::HashCombine(elementsNum,
								flags,
								bindingDescriptorType);
	}

	Uint32					elementsNum;
	EBindingFlags			flags;
	rhi::EDescriptorType	bindingDescriptorType;
};


struct TextureBindingData : public CommonBindingData
{
	explicit TextureBindingData(Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
	{ }

	void PostInitialize()
	{
		bindingDescriptorType = priv::SelectTextureDescriptorType(flags);
	}
};


struct CombinedTextureSamplerBindingData : public CommonBindingData
{
	explicit CombinedTextureSamplerBindingData(Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
	{ }

	void PostInitialize()
	{
		bindingDescriptorType = rhi::EDescriptorType::CombinedTextureSampler;
	}
};


struct BufferBindingData : public CommonBindingData
{
	explicit BufferBindingData(Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
		, m_size(0) // invalid
	{ }

	void PostInitialize()
	{
		bindingDescriptorType = priv::SelectBufferDescriptorType(flags);
	}

	bool TryMergeWithOtherBinding(const BufferBindingData& other)
	{
		if (m_size == other.m_size)
		{
			return CommonBindingData::TryMergeWithOtherBinding(other);
		}

		return false;
	}

	void SetSize(Uint32 size)
	{
		SPT_CHECK(!IsUnbound());
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

	Uint32 GetSize() const
	{
		SPT_CHECK(!IsUnbound());
		return m_size;
	}

	SizeType Hash() const
	{
		return lib::HashCombine(CommonBindingData::Hash(),
								m_size);
	}

private:

	Uint32			m_size;
};


struct SamplerBindingData : public CommonBindingData
{
public:

	explicit SamplerBindingData(Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
	{ }

	void PostInitialize()
	{
		bindingDescriptorType = rhi::EDescriptorType::Sampler;
	}

	bool TryMergeWithOtherBinding(const SamplerBindingData& other)
	{
		if (IsImmutable() == other.IsImmutable())
		{
			if (!IsImmutable() || GetImmutableSamplerDefinition() == other.GetImmutableSamplerDefinition())
			{
				return CommonBindingData::TryMergeWithOtherBinding(other);
			}
		}

		return false;
	}

	Bool IsImmutable() const
	{
		return lib::HasAnyFlag(flags, EBindingFlags::ImmutableSampler);
	}

	void SetImmutableSamplerDefinition(const rhi::SamplerDefinition& samplerDef)
	{
		m_immutableSamplerDefinition = samplerDef;
	}
	
	const rhi::SamplerDefinition& GetImmutableSamplerDefinition() const
	{
		return m_immutableSamplerDefinition;
	}

private:

	rhi::SamplerDefinition m_immutableSamplerDefinition;
};


struct AccelerationStructureBindingData : public CommonBindingData
{
public:

	explicit AccelerationStructureBindingData(Uint32 inElementsNum = 1, EBindingFlags inFlags = priv::defaultBindingFlags)
		: CommonBindingData(inElementsNum, inFlags)
	{ }

	void PostInitialize()
	{
		bindingDescriptorType = rhi::EDescriptorType::AccelerationStructure;
	}
};


// should be in the same order as EBindingType
using BindingDataVariant = std::variant<TextureBindingData,
										CombinedTextureSamplerBindingData,
										BufferBindingData,
										SamplerBindingData,
										AccelerationStructureBindingData>;


struct GenericShaderBinding
{
	GenericShaderBinding()
	{
		std::visit([](auto& data)
				   {
					   return data.AddFlag(EBindingFlags::Invalid);
				   },
				   m_data);
	}

	template<typename TBindingDataType>
	explicit GenericShaderBinding(TBindingDataType bindingData)
		: m_data(bindingData)
	{ }

	void PostInitialize()
	{
		return std::visit([](auto& data)
						  {
							  return data.PostInitialize();
						  },
						  m_data);
	}

	template<typename TBindingDataType>
	void Set(TBindingDataType data)
	{
		m_data = data;
	}

	bool TryMergeWith(const GenericShaderBinding& other)
	{
		if (!other.IsValid())
		{
			return true;
		}

		if (!IsValid())
		{
			*this = other;
			return true;
		}

		if (other.IsValid() && GetBindingType() == other.GetBindingType())
		{
			return std::visit([&other](auto& data)
							  {
								  using DataType = std::decay_t<decltype(data)>;
								  return data.TryMergeWithOtherBinding(other.As<DataType>());
							  },
							  m_data);
		}

		return false;
	}

	void AddShaderStage(rhi::EShaderStage stage)
	{
		return std::visit([stage](auto& data)
						  {
							  return data.AddShaderStage(stage);
						  },
						  m_data);
	}

	void AddShaderStages(rhi::EShaderStageFlags stages)
	{
		return std::visit([stages](auto& data)
						  {
							  return data.AddShaderStages(stages);
						  },
						  m_data);
	}

	void AddFlags(EBindingFlags flags)
	{
		return std::visit([flags](auto& data)
						  {
							  return data.AddFlag(flags);
						  },
						  m_data);
	}

	EBindingFlags GetFlags() const
	{
		return std::visit([](const auto& data)
						  {
							  return data.flags;
						  },
						  m_data);
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
		return std::visit([](const auto data)
						  {
							  return data.IsValid();
						  },
						  m_data);
	}

	EBindingType GetBindingType() const
	{
		return static_cast<EBindingType>(m_data.index());
	}

	rhi::EDescriptorType GetDescriptorType() const
	{
		return std::visit([](const auto data)
						  {
							  return data.GetDescriptorType();
						  },
						  m_data);
	}

	const BindingDataVariant& GetBindingData() const
	{
		return m_data;
	}

	SizeType Hash() const
	{
		if (IsValid())
		{
			return lib::HashCombine(GetBindingType(),
									std::visit([](const auto& data) { return data.Hash(); }, m_data));
		}
		return 0;
	}

private:

	BindingDataVariant m_data;
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

		if (!m_bindings[newBindingIdx].IsValid())
		{
			SPT_CHECK(bindingData.IsValid());

			m_bindings[newBindingIdx].Set(bindingData);
		}
		else
		{
			SPT_CHECK(m_bindings[newBindingIdx].Contains<TBindingDataType>());
		}
	}

	void PostInitialize()
	{
		std::for_each(std::begin(m_bindings), std::end(m_bindings),
					  [](GenericShaderBinding& binding)
					  {
						  if (binding.IsValid())
						  {
							  binding.PostInitialize();
						  }
					  });
	}

	void MergeWith(const ShaderDescriptorSet& other)
	{
		const SizeType bindingsNum = std::max(m_bindings.size(), other.m_bindings.size());
		m_bindings.resize(bindingsNum);

		for (SizeType bindingIdx = 0; bindingIdx < other.m_bindings.size(); ++bindingIdx)
		{
			if (!m_bindings[bindingIdx].TryMergeWith(other.m_bindings[bindingIdx]))
			{
				SPT_LOG_ERROR(DefaultLog, "Failed to merge shader descriptor set bindings");
			}
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
	ShaderParamEntryCommon(Uint32 inSetIdx = idxNone<Uint32>, Uint32 inBindingIdx = idxNone<Uint32>)
		: setIdx(inSetIdx)
		, bindingIdx(inBindingIdx)
	{ }

	Bool IsValid() const
	{
		return setIdx != idxNone<Uint32> && bindingIdx != idxNone<Uint32>;
	}

	Uint32		setIdx;
	Uint32		bindingIdx;
};


struct ShaderTextureParamEntry : public ShaderParamEntryCommon
{
	ShaderTextureParamEntry() = default;

	ShaderTextureParamEntry(Uint32 inSetIdx, Uint32 inBindingIdx)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
	{ }

	// No additional data for now
};


struct ShaderCombinedTextureSamplerParamEntry : public ShaderParamEntryCommon
{
	ShaderCombinedTextureSamplerParamEntry() = default;

	ShaderCombinedTextureSamplerParamEntry(Uint32 inSetIdx, Uint32 inBindingIdx)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
	{ }

	// No additional data for now
};


struct ShaderBufferParamEntry : public ShaderParamEntryCommon
{
	ShaderBufferParamEntry() = default;

	explicit ShaderBufferParamEntry(Uint32 inSetIdx, Uint32 inBindingIdx)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
	{ }

	// No additional data for now
};


struct ShaderSamplerParamEntry : public ShaderParamEntryCommon
{
	ShaderSamplerParamEntry() = default;

	explicit ShaderSamplerParamEntry(Uint32 inSetIdx, Uint32 inBindingIdx)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
	{ }

	// No additional data for now
};


struct ShaderAccelerationStructureParamEntry : public ShaderParamEntryCommon
{
	ShaderAccelerationStructureParamEntry() = default;

	explicit ShaderAccelerationStructureParamEntry(Uint32 inSetIdx, Uint32 inBindingIdx)
		: ShaderParamEntryCommon(inSetIdx, inBindingIdx)
	{ }

	// No additional data for now
};


using ShaderParamEntryVariant = std::variant<ShaderTextureParamEntry,
											 ShaderCombinedTextureSamplerParamEntry,
											 ShaderBufferParamEntry,
											 ShaderSamplerParamEntry,
											 ShaderAccelerationStructureParamEntry>;


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
		return Contains<TEntryDataType>() ? As<TEntryDataType>() : TEntryDataType();
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

	Uint32 GetSetIdx() const
	{
		return std::visit(
			[](const auto data) -> Uint8
			{
				return static_cast<Uint8>(data.setIdx);
			},
			m_data);
	}

	Uint32 GetBindingIdx() const
	{
		return std::visit(
			[](const auto data) -> Uint8
			{
				return static_cast<Uint8>(data.bindingIdx);
			},
			m_data);
	}

private:

	ShaderParamEntryVariant		m_data;
};


struct ShaderDataParam
{
	ShaderDataParam() = default;

	ShaderDataParam(Uint16 inOffset, Uint16 inSize, Uint16 inStride)
		: offset(inOffset)
		, size(inSize)
		, stride(inStride)
	{ }

	Uint16		offset;
	Uint16		size;
	Uint16		stride;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

inline SizeType HashDescriptorSetBinding(const GenericShaderBinding& binding, const lib::HashedString& paramName)
{
	SPT_PROFILER_FUNCTION();

	return lib::HashCombine(binding.Hash(), paramName);
}

} // spt::smd
