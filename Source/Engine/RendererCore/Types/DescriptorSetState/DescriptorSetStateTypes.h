#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
#include "ShaderMetaDataTypes.h"
#include "Utility/Templates/Callable.h"
#include "Utility/NamedType.h"
#include "RendererUtils.h"


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


class RENDERER_CORE_API DescriptorSetUpdateContext
{
public:

	DescriptorSetUpdateContext(rhi::RHIDescriptorSet descriptorSet, DescriptorSetWriter& writer, const DescriptorSetLayout& dsLayout);

	void UpdateBuffer(Uint32 bindingIdx, const BufferView& buffer) const;
	void UpdateBuffer(Uint32 bindingIdx, const BufferView& buffer, const BufferView& countBuffer) const;
	void UpdateTexture(Uint32 bindingIdx, const lib::SharedRef<TextureView>& texture, Uint32 arrayIndex = 0) const;
	void UpdateAccelerationStructure(Uint32 bindingIdx, const lib::SharedRef<TopLevelAS>& tlas) const;

	void AddBindingsOffset(Uint32 offset);
	void RemoveBindingsOffset(Uint32 offset);

private:

	rhi::WriteDescriptorDefinition CreateWriteDescriptorDefinition(Uint32 bindingIdx, Uint32 arrayIdx = 0) const;

	Uint32 ComputeFinalBindingIdx(Uint32 bindingIdx) const;

	rhi::RHIDescriptorSet      m_descriptorSet;
	DescriptorSetWriter&       m_writer;
	const DescriptorSetLayout& m_dsLayout;

	Uint32 m_baseBindingIdx;
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

} // spt::rdr
