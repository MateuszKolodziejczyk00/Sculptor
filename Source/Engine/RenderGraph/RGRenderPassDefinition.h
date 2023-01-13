#pragma once

#include "RenderGraphMacros.h"
#include "RGResources/RGResourceHandles.h"
#include "RHICore/Commands/RHIRenderingDefinition.h"
#include "CommandsRecorder/RenderingDefinition.h"



namespace spt::rg
{

class RGDependenciesBuilder;

using RGRenderTargetDef = rhi::RTGenericDefinition<RGTextureViewHandle>;


class RENDER_GRAPH_API RGRenderPassDefinition
{
public:

	RGRenderPassDefinition(math::Vector2i renderAreaOffset, math::Vector2u renderAreaExtent, rhi::ERenderingFlags renderingFlags = rhi::ERenderingFlags::Default);
	
	SPT_NODISCARD RGRenderTargetDef& AddColorRenderTarget();
	void AddColorRenderTarget(const RGRenderTargetDef& definition);

	SPT_NODISCARD RGRenderTargetDef& GetDepthRenderTargetRef();
	void SetDepthRenderTarget(const RGRenderTargetDef& definition);

	SPT_NODISCARD RGRenderTargetDef& GetStencilRenderTargetRef();
	void SetStencilRenderTarget(const RGRenderTargetDef& definition);

	rdr::RenderingDefinition CreateRenderingDefinition() const;

	void BuildDependencies(RGDependenciesBuilder& dependenciesBuilder) const;

private:

	rdr::RTDefinition CreateRTDefinition(const RGRenderTargetDef& rgDef) const;

	Bool IsRTDefinitionValid(const RGRenderTargetDef& rgDef) const;

	lib::DynamicArray<RGRenderTargetDef>	m_colorRenderTargetDefs;
	RGRenderTargetDef						m_depthRenderTargetDef;
	RGRenderTargetDef						m_stencilRenderTargetDef;

	math::Vector2i			m_renderAreaOffset;
	math::Vector2u			m_renderAreaExtent;
	rhi::ERenderingFlags	m_renderingFlags;
};

} // spt::rg
