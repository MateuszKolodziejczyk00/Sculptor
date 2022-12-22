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
	rhi::EShaderStageFlags	shaderStages;
};


struct RGBufferAccessDef
{
	RGBufferViewHandle		resource;
	ERGBufferAccess			access;
	rhi::EShaderStageFlags	shaderStages;
};


struct RGDependeciesContainer
{
	lib::DynamicArray<RGTextureAccessDef> textureAccesses;
	lib::DynamicArray<RGBufferAccessDef>  bufferAccesses;
};


class RENDER_GRAPH_API RGDependenciesBuilder
{
public:

	explicit RGDependenciesBuilder(RenderGraphBuilder& graphBuilder, RGDependeciesContainer& dependecies);

	void AddTextureAccess(RGTextureViewHandle texture, ERGTextureAccess access, rhi::EShaderStageFlags shaderStages = rhi::EShaderStageFlags::None);
	void AddTextureAccess(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, rhi::EShaderStageFlags shaderStages = rhi::EShaderStageFlags::None);

	void AddBufferAccess(RGBufferViewHandle buffer, ERGBufferAccess access, rhi::EShaderStageFlags shaderStages = rhi::EShaderStageFlags::None);
	void AddBufferAccess(const lib::SharedRef<rdr::BufferView>& buffer, ERGBufferAccess access, rhi::EShaderStageFlags shaderStages = rhi::EShaderStageFlags::None);

private:

	RenderGraphBuilder& m_graphBuilder;
	RGDependeciesContainer& m_dependeciesRef;
};

} // spt::rg
