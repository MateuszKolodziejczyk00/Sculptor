#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RGResources/RGResources.h"
#include "RHICore/RHIShaderTypes.h"
#include "ShaderStructs/ShaderStructs.h"


namespace spt::rg
{

class RenderGraphBuilder;


struct RGTextureAccessDef
{
	RGTextureViewHandle textureView;
	ERGTextureAccess    access;
	rhi::EPipelineStage pipelineStages;
};


struct RGBufferAccessDef
{
	RGBufferViewHandle  resource;
	ERGBufferAccess     access;
	rhi::EPipelineStage pipelineStages;

#if DEBUG_RENDER_GRAPH
	lib::HashedString structTypeName;
	Uint32            elementsNum = 1u;
#endif // DEBUG_RENDER_GRAPH
};


struct RGDependeciesContainer
{
	RGDependeciesContainer() = default;

	Bool HasAnyDependencies() const
	{
		return !textureAccesses.empty() || !bufferAccesses.empty();
	}

	lib::DynamicArray<RGTextureAccessDef> textureAccesses;
	lib::DynamicArray<RGBufferAccessDef>  bufferAccesses;
};


struct RGDependencyStages
{
	RGDependencyStages()
		: pipelineStages(rhi::EPipelineStage::None)
		, shouldUseDefaultStages(true)
	{ }
	
	RGDependencyStages(rhi::EPipelineStage inStages)
		: pipelineStages(inStages)
		, shouldUseDefaultStages(false)
	{ }
	
	RGDependencyStages(rhi::EShaderStageFlags shaderStages)
		: pipelineStages(rhi::EPipelineStage::None)
		, shouldUseDefaultStages(false)
	{
		if (lib::HasAnyFlag(shaderStages, rhi::EShaderStageFlags::Vertex))
		{
			lib::AddFlag(pipelineStages, rhi::EPipelineStage::VertexShader);
		}
		if (lib::HasAnyFlag(shaderStages, rhi::EShaderStageFlags::Fragment))
		{
			lib::AddFlag(pipelineStages, rhi::EPipelineStage::FragmentShader);
		}
		if (lib::HasAnyFlag(shaderStages, rhi::EShaderStageFlags::Compute))
		{
			lib::AddFlag(pipelineStages, rhi::EPipelineStage::ComputeShader);
		}
		if (   lib::HasAnyFlag(shaderStages, rhi::EShaderStageFlags::RTGeneration)
			|| lib::HasAnyFlag(shaderStages, rhi::EShaderStageFlags::RTAnyHit)
			|| lib::HasAnyFlag(shaderStages, rhi::EShaderStageFlags::RTClosestHit)
			|| lib::HasAnyFlag(shaderStages, rhi::EShaderStageFlags::RTMiss)
			|| lib::HasAnyFlag(shaderStages, rhi::EShaderStageFlags::RTIntersection))
		{
			lib::AddFlag(pipelineStages, rhi::EPipelineStage::RayTracingShader);
		}
	}

	rhi::EPipelineStage pipelineStages;

	Bool shouldUseDefaultStages;
};


struct RGBufferAccessInfo
{
	RGBufferAccessInfo() = default;

	RGBufferAccessInfo(ERGBufferAccess inAccess)
		: access(inAccess)
	{ }

	ERGBufferAccess access = ERGBufferAccess::Unknown;


#if DEBUG_RENDER_GRAPH
	lib::HashedString structTypeName;
	Uint32            elementsNum = 1u;
#endif // DEBUG_RENDER_GRAPH
};


class RENDER_GRAPH_API RGDependenciesBuilder
{
public:

	explicit RGDependenciesBuilder(RenderGraphBuilder& graphBuilder, RGDependeciesContainer& dependecies, rhi::EPipelineStage defaultStages = rhi::EPipelineStage::None);

	void AddTextureAccess(RGTextureViewHandle texture, ERGTextureAccess access, RGDependencyStages dependencyStages = RGDependencyStages());
	void AddTextureAccess(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, RGDependencyStages dependencyStages = RGDependencyStages());
	void AddTextureAccessIfAcquired(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, RGDependencyStages dependencyStages = RGDependencyStages());
	void AddTextureAccess(rdr::ResourceDescriptorIdx textureDescriptor, ERGTextureAccess access, RGDependencyStages dependencyStages = RGDependencyStages());

	void AddBufferAccess(RGBufferViewHandle buffer, const RGBufferAccessInfo& access, RGDependencyStages dependencyStages = RGDependencyStages());
	void AddBufferAccess(const rdr::BufferView& buffer, const RGBufferAccessInfo& access, RGDependencyStages dependencyStages = RGDependencyStages());

private:

	RenderGraphBuilder& m_graphBuilder;
	RGDependeciesContainer& m_dependeciesRef;

	rhi::EPipelineStage m_defaultStages;
};


template<typename TType>
void CollectStructDependencies(lib::Span<const Byte> hlsl, RGDependenciesBuilder& dependenciesBuilder);


namespace priv
{

template<typename TShaderStructMemberMetaData>
void CollectMembersDependencies(lib::Span<const Byte> hlsl, RGDependenciesBuilder& dependenciesBuilder)
{
	if constexpr (!rdr::shader_translator::priv::IsTailMember<TShaderStructMemberMetaData>())
	{
		CollectMembersDependencies<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(hlsl, dependenciesBuilder);
	}

	using TMemberType = typename TShaderStructMemberMetaData::UnderlyingType;

	if constexpr (!rdr::shader_translator::priv::IsHeadMember<TShaderStructMemberMetaData>())
	{
		constexpr Uint32 hlslMemberOffset = TShaderStructMemberMetaData::s_hlsl_memberOffset;
		constexpr Uint32 hlslMemberSize = TShaderStructMemberMetaData::s_hlsl_memberSize;

		CollectStructDependencies<TMemberType>(hlsl.subspan(hlslMemberOffset, hlslMemberSize), dependenciesBuilder);
	}
}

} // priv


template<typename TType>
struct HLSLStructDependenciesBuider
{
	static void CollectDependencies(lib::Span<const Byte> hlsl, RGDependenciesBuilder& dependenciesBuilder)
	{
		if constexpr (lib::CContainer<TType>)
		{
			using TArrayTraits = lib::StaticArrayTraits<TType>;
			using TElemType = typename TArrayTraits::Type;

			constexpr Uint32 hlslTypeSize    = rdr::shader_translator::HLSLSizeOf<TElemType>();
			constexpr Uint32 hlslElementSize = hlslTypeSize >= 16u ? hlslTypeSize : 16u; // HLSL requires at least 16 bytes for each element of array

			for (SizeType idx = 0u; idx < TArrayTraits::Size; ++idx)
			{
				CollectStructDependencies<TElemType>(hlsl.subspan(idx * hlslElementSize, hlslTypeSize), dependenciesBuilder);
			}
		}
		else if constexpr (rdr::shader_translator::IsShaderStruct<TType>())
		{
			priv::CollectMembersDependencies<typename TType::HeadMemberMetaData>(hlsl, dependenciesBuilder);
		}

		// for other types, we do nothing here.
		// If there is some logic needed for some type, it should specialize this template
	}
};


template<typename TType>
void CollectStructDependencies(lib::Span<const Byte> hlsl, RGDependenciesBuilder& dependenciesBuilder)
{
	HLSLStructDependenciesBuider<TType>::CollectDependencies(hlsl, dependenciesBuilder);
}

} // spt::rg
