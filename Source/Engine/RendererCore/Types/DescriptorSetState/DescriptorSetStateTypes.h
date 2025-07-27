#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderMetaDataTypes.h"
#include "Utility/Templates/Callable.h"
#include "Utility/NamedType.h"
#include "RendererUtils.h"
#include "Types/DescriptorSetLayout.h"
#include "RHIBridge/RHIImpl.h"


namespace spt::smd
{
class ShaderMetaData;
struct GenericShaderBinding;
} // spt::smd


namespace spt::rdr
{

class TopLevelAS;
class TextureView;
class BufferView;
class DescriptorSetWriter;
class DescriptorSetStateLayoutsRegistry;
class DescriptorSetLayout;


//struct DSStateIDTag {};
//using DSStateID = lib::NamedType<Uint64, DSStateIDTag>;
using DSStateID = SizeType;

struct DSStateTypeIDTag {};
using DSStateTypeID = lib::NamedType<SizeType, DSStateTypeIDTag>;


enum class EDescriptorSetStateFlags
{
	None       = 0,

	Default    = None
};


struct ShaderBindingMetaData
{
	constexpr ShaderBindingMetaData(rhi::EDescriptorType inType = rhi::EDescriptorType::None, smd::EBindingFlags inFlags = smd::EBindingFlags::None, Uint32 arraySize = 1u)
		: type(inType)
		, arraySize(arraySize)
		, flags(inFlags)
	{ }
	
	constexpr ShaderBindingMetaData(const rhi::SamplerDefinition& inImmutableSamplerDef, smd::EBindingFlags inFlags = smd::EBindingFlags::ImmutableSampler)
		: type(rhi::EDescriptorType::Sampler)
		, flags(inFlags)
		, arraySize(1u)
		, immutableSamplerDef(inImmutableSamplerDef)
	{ }

	rhi::EDescriptorType type;

	smd::EBindingFlags flags;

	Uint32 arraySize;

	std::optional<rhi::SamplerDefinition> immutableSamplerDef;
};


class ShaderCompilationAdditionalArgsBuilder
{
public:

	explicit ShaderCompilationAdditionalArgsBuilder(lib::DynamicArray<lib::HashedString>& macroDefinitions)
		: m_macroDefinitions(macroDefinitions)
	{  }

	void AddDescriptorSet(const lib::HashedString& dsName)
	{
		m_macroDefinitions.emplace_back("DS_" + dsName.ToString());
	}

private:

	lib::DynamicArray<lib::HashedString>& m_macroDefinitions;
};


struct DescriptorSetStateLayoutDefinition
{
	lib::Span<const ShaderBindingMetaData> bindings;
};


using DescriptorSetStateLayoutFactoryMethod = lib::RawCallable<void(DescriptorSetStateLayoutsRegistry&)>;


class RENDERER_CORE_API DescriptorSetStateLayoutsRegistry
{
public:

	static DescriptorSetStateLayoutsRegistry& Get();

	void CreateRegisteredLayouts();
	void ReleaseRegisteredLayouts();

	void RegisterFactoryMethod(DSStateTypeID dsTypeID, DescriptorSetStateLayoutFactoryMethod layoutFactoryMethod);
	void RegisterLayout(DSStateTypeID dsTypeID, const RendererResourceName& name, const DescriptorSetStateLayoutDefinition& layoutDef);

	const lib::SharedPtr<DescriptorSetLayout>& GetLayoutChecked(DSStateTypeID dsTypeID) const;

private:

	Bool ShouldCreateLayout(DSStateTypeID dsTypeID, const RendererResourceName& name, const DescriptorSetStateLayoutDefinition& layoutDef) const;
	void CreateLayout(DSStateTypeID dsTypeID, const RendererResourceName& name, const DescriptorSetStateLayoutDefinition& layoutDef);

	Bool IsRayTracingLayout(DSStateTypeID dsTypeID, const RendererResourceName& name, const DescriptorSetStateLayoutDefinition& layoutDef) const;

	DescriptorSetStateLayoutsRegistry();

	lib::HashMap<DSStateTypeID, lib::SharedPtr<DescriptorSetLayout>> m_layouts;

	lib::HashMap<DSStateTypeID, DescriptorSetStateLayoutFactoryMethod> m_layoutFactoryMethods;
};


using DescriptorsAllocation = rhi::RHIVirtualAllocation;


class DescriptorArrayIndexer
{
public:

	DescriptorArrayIndexer(Byte* data, Uint32 stride, Uint32 elementsNum)
		: m_data(data)
		, m_stride(stride)
		, m_elementsNum(elementsNum)
	{ }

	Byte* operator[](Uint32 index) const
	{
		SPT_CHECK(index < m_elementsNum);
		return m_data + (index * m_stride);
	}

private:

	Byte*  m_data;
	Uint32 m_stride;
	Uint32 m_elementsNum;
};


class DescriptorSetIndexer
{
public:

	DescriptorSetIndexer(lib::Span<Byte> descriptorsData, DescriptorSetLayout& layout)
		: m_descriptorsData(descriptorsData)
		, m_layout(layout)
		, m_baseBindingIdx(0)
	{ }

	void AddBindingsOffset(Uint32 offset)
	{
		m_baseBindingIdx += offset;
	}

	void RemoveBindingsOffset(Uint32 offset)
	{
		SPT_CHECK(m_baseBindingIdx >= offset);
		m_baseBindingIdx -= offset;
	}

	DescriptorArrayIndexer operator[](Uint32 bindingIdx) const
	{
		const rhi::DescriptorProps descriptorProps = rhi::RHI::GetDescriptorProps();

		const Uint32 binding = m_baseBindingIdx + bindingIdx;
		const Uint64 descriptorsOffset = m_layout.GetRHI().GetDescriptorOffset(binding);

		const DescriptorBindingInfo bindingInfo = m_layout.GetBindingInfo(binding);

		const Uint32 elementsNum = bindingInfo.arraySize;
		const Uint32 stride      = descriptorProps.StrideFor(bindingInfo.descriptorType);

		SPT_CHECK(descriptorsOffset + (elementsNum * stride) <= m_descriptorsData.size());

		return DescriptorArrayIndexer(
			m_descriptorsData.data() + descriptorsOffset,
			stride,
			elementsNum
		);
	}

private:

	lib::Span<Byte>      m_descriptorsData;
	DescriptorSetLayout& m_layout;
	Uint32               m_baseBindingIdx = 0;
};


class RENDERER_CORE_API DescriptorStackAllocator
{
public:

	explicit DescriptorStackAllocator(Uint32 stackSize);
	~DescriptorStackAllocator();

	rhi::RHIDescriptorRange AllocateRange(Uint32 size);

private:

	rhi::RHIDescriptorRange m_descriptorRange;
	std::atomic<Uint32> m_currentOffset = 0u;
};

} // spt::rdr
