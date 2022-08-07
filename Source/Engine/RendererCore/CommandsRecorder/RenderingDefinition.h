#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/Commands/RHIRenderingDefinition.h"
#include "Types/Texture.h"


namespace spt::renderer
{

using RTDefinition = rhi::RTGenericDefinition<lib::SharedPtr<TextureView>>;


class RenderingDefinition
{
public:

	RenderingDefinition(Flags32 renderingFlags, math::Vector2i renderAreaOffset, math::Vector2u renderAreaExtent)
	{
		m_rhiDefinition.m_renderingFlags = renderingFlags;
		m_rhiDefinition.m_renderAreaOffset = renderAreaOffset;
		m_rhiDefinition.m_renderAreaExtent = renderAreaExtent;
	}

	void							AddColorRenderTarget(const RTDefinition& renderTarget)
	{
		m_rhiDefinition.m_colorRTs.push_back(GetRTDefRHI(renderTarget));
	}

	void							AddDepthRenderTarget(const RTDefinition& renderTarget)
	{
		m_rhiDefinition.m_depthRT = GetRTDefRHI(renderTarget);
	}

	void							AddStencilRenderTarget(const RTDefinition& renderTarget)
	{
		m_rhiDefinition.m_stencilRT = GetRTDefRHI(renderTarget);
	}

	const rhi::RenderingDefinition& GetRHI() const
	{
		return m_rhiDefinition;
	}

private:

	rhi::RHIRenderTargetDefinition				GetRTDefRHI(const RTDefinition& renderTarget) const
	{
		SPT_CHECK(!!renderTarget.m_textureView);

		rhi::RHIRenderTargetDefinition rhiDef;
		rhiDef.m_textureView			= renderTarget.m_textureView->GetRHI();

		if (renderTarget.m_resolveTextureView)
		{
			rhiDef.m_resolveTextureView = renderTarget.m_resolveTextureView->GetRHI();
		}

		rhiDef.m_loadOperation			= renderTarget.m_loadOperation;
		rhiDef.m_storeOperation			= renderTarget.m_storeOperation;
		rhiDef.m_resolveMode			= renderTarget.m_resolveMode;
		rhiDef.m_clearColor				= renderTarget.m_clearColor;

		return rhiDef;
	}

	rhi::RenderingDefinition		m_rhiDefinition;
};

}