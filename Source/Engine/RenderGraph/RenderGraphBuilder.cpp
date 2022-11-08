#include "RenderGraphBuilder.h"

namespace spt::rg
{

RenderGraphBuilder::RenderGraphBuilder()
	: resourceIDCounter(1)
{ }

RGTextureHandle RenderGraphBuilder::AcquireExternalTexture(lib::SharedPtr<rdr::Texture> texture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!texture);

	const RGResourceID textureResourceID = resourceIDCounter++;

	const RenderGraphDebugName name = RG_DEBUG_NAME(texture->GetRHI().GetName());

	RGResourceDef definition;
	definition.name = name;
	definition.id = textureResourceID;
	definition.flags = lib::Flags(ERGResourceFlags::Default, ERGResourceFlags::External);

	m_textureResources.emplace(textureResourceID, RGTexture(definition, texture));

	m_externalTextures.emplace(std::move(texture), textureResourceID);

	return RGTextureHandle(textureResourceID, name);
}

RGTextureHandle RenderGraphBuilder::CreateTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	const RGResourceID textureResourceID = resourceIDCounter++;

	RGResourceDef definition;
	definition.name = name;
	definition.id = textureResourceID;
	definition.flags = flags;

	m_textureResources.emplace(textureResourceID, RGTexture(definition, textureDefinition, allocationInfo));

	return RGTextureHandle(textureResourceID, name);
}

void RenderGraphBuilder::ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination)
{
	SPT_PROFILER_FUNCTION();

	RGTexture& texture = m_textureResources.at(textureHandle.GetID());
	texture.SetExtractionDestination(extractDestination);

	m_extractedTextures.emplace_back(textureHandle);
}

} // spt::rg
