#include "RGRenderPassDefinition.h"
#include "DependenciesBuilder.h"

namespace spt::rg
{

RGRenderPassDefinition::RGRenderPassDefinition(math::Vector2i renderAreaOffset, math::Vector2u renderAreaExtent, rhi::ERenderingFlags renderingFlags /*= rhi::ERenderingFlags::Default*/)
	: m_renderAreaOffset(renderAreaOffset)
	, m_renderAreaExtent(renderAreaExtent)
	, m_renderingFlags(renderingFlags)
{ }

RGRenderTargetDef& RGRenderPassDefinition::AddColorRenderTarget()
{
	return m_colorRenderTargetDefs.emplace_back();
}

void RGRenderPassDefinition::AddColorRenderTarget(const RGRenderTargetDef& definition)
{
	SPT_CHECK(!!definition.textureView);
	definition.textureView->GetTexture()->AddUsageForAccess(ERGTextureAccess::ColorRenderTarget);
	m_colorRenderTargetDefs.emplace_back(definition);
}

RGRenderTargetDef& RGRenderPassDefinition::GetDepthRenderTargetRef()
{
	return m_depthRenderTargetDef;
}

void RGRenderPassDefinition::SetDepthRenderTarget(const RGRenderTargetDef& definition)
{
	SPT_CHECK(!!definition.textureView);
	definition.textureView->GetTexture()->AddUsageForAccess(ERGTextureAccess::DepthRenderTarget);
	m_depthRenderTargetDef = definition;
}

RGRenderTargetDef& RGRenderPassDefinition::GetStencilRenderTargetRef()
{
	return m_stencilRenderTargetDef;
}

void RGRenderPassDefinition::SetStencilRenderTarget(const RGRenderTargetDef& definition)
{
	m_stencilRenderTargetDef = definition;
}

rdr::RenderingDefinition RGRenderPassDefinition::CreateRenderingDefinition() const
{
	rdr::RenderingDefinition renderingDefinition(m_renderAreaOffset, m_renderAreaExtent, m_renderingFlags);

	std::for_each(std::cbegin(m_colorRenderTargetDefs), std::cend(m_colorRenderTargetDefs),
				  [this, &renderingDefinition](const RGRenderTargetDef& def)
				  {
					  renderingDefinition.AddColorRenderTarget(CreateRTDefinition(def));
				  });

	if (IsRTDefinitionValid(m_depthRenderTargetDef))
	{
		renderingDefinition.AddDepthRenderTarget(CreateRTDefinition(m_depthRenderTargetDef));
	}

	if (IsRTDefinitionValid(m_stencilRenderTargetDef))
	{
		renderingDefinition.AddStencilRenderTarget(CreateRTDefinition(m_stencilRenderTargetDef));
	}

	return renderingDefinition;
}

void RGRenderPassDefinition::BuildDependencies(RGDependenciesBuilder& dependenciesBuilder) const
{
	for (const RGRenderTargetDef& colorRenderTarget : m_colorRenderTargetDefs)
	{
		SPT_CHECK(colorRenderTarget.textureView.IsValid());

		dependenciesBuilder.AddTextureAccess(colorRenderTarget.textureView, ERGTextureAccess::ColorRenderTarget);

		if (colorRenderTarget.resolveTextureView.IsValid())
		{
			dependenciesBuilder.AddTextureAccess(colorRenderTarget.resolveTextureView, ERGTextureAccess::ColorRenderTarget);
		}
	}

	if (m_depthRenderTargetDef.textureView.IsValid())
	{
		dependenciesBuilder.AddTextureAccess(m_depthRenderTargetDef.textureView, ERGTextureAccess::DepthRenderTarget);
	}

	if (m_stencilRenderTargetDef.textureView.IsValid())
	{
		dependenciesBuilder.AddTextureAccess(m_stencilRenderTargetDef.textureView, ERGTextureAccess::StencilRenderTarget);
	}
}

rdr::RTDefinition RGRenderPassDefinition::CreateRTDefinition(const RGRenderTargetDef& rgDef) const
{
	SPT_CHECK(rgDef.textureView.IsValid());

	rdr::RTDefinition def;

	def.textureView = rgDef.textureView->GetResource();

	if (rgDef.resolveTextureView.IsValid())
	{
		def.resolveTextureView = rgDef.resolveTextureView->GetResource();
	}

	def.loadOperation	= rgDef.loadOperation;
	def.storeOperation	= rgDef.storeOperation;
	def.resolveMode		= rgDef.resolveMode;
	def.clearColor		= rgDef.clearColor;

	return def;
}

Bool RGRenderPassDefinition::IsRTDefinitionValid(const RGRenderTargetDef& rgDef) const
{
	return rgDef.textureView.IsValid();
}

} // spt::rg
