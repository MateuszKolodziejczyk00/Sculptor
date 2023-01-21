#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RGResources/RGResources.h"
#include "RHICore/RHIShaderTypes.h"


namespace spt::rg
{

class RenderGraphBuilder;


struct RGTextureAccessDef
{
	RGTextureViewHandle		textureView;
	ERGTextureAccess		access;
	rhi::EPipelineStage		pipelineStages;
};


struct RGBufferAccessDef
{
	RGBufferViewHandle		resource;
	ERGBufferAccess			access;
	rhi::EPipelineStage		pipelineStages;
};


struct RGDependeciesContainer
{
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
	}

	rhi::EPipelineStage pipelineStages;

	Bool shouldUseDefaultStages;
};


class RENDER_GRAPH_API RGDependenciesBuilder
{
public:

	explicit RGDependenciesBuilder(RenderGraphBuilder& graphBuilder, RGDependeciesContainer& dependecies, rhi::EPipelineStage defaultStages = rhi::EPipelineStage::None);

	void AddTextureAccess(RGTextureViewHandle texture, ERGTextureAccess access, RGDependencyStages dependencyStages = RGDependencyStages());
	void AddTextureAccess(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, RGDependencyStages dependencyStages = RGDependencyStages());

	void AddBufferAccess(RGBufferViewHandle buffer, ERGBufferAccess access, RGDependencyStages dependencyStages = RGDependencyStages());
	void AddBufferAccess(const rdr::BufferView& buffer, ERGBufferAccess access, RGDependencyStages dependencyStages = RGDependencyStages());

private:

	RenderGraphBuilder& m_graphBuilder;
	RGDependeciesContainer& m_dependeciesRef;

	rhi::EPipelineStage m_defaultStages;
};

} // spt::rg
