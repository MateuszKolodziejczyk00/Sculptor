#include "RenderGraphBuilder.h"

namespace spt::rg
{

RenderGraphBuilder::RenderGraphBuilder()
{ }

rg::RGTextureHandle RenderGraphBuilder::AcquireExternalTexture(lib::SharedPtr<rdr::Texture> texture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY();

	return RGTextureHandle{};
}

RGTextureHandle RenderGraphBuilder::CreateTexture(const RenderGraphDebugName& m_name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY();

	return RGTextureHandle{};
}

void RenderGraphBuilder::ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination)
{

}

} // spt::rg
