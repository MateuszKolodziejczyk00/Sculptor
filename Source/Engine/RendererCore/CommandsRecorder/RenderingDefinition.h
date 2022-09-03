#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/Commands/RHIRenderingDefinition.h"
#include "Types/Texture.h"


namespace spt::rdr
{

using RTDefinition = rhi::RTGenericDefinition<lib::SharedPtr<TextureView>>;


class RenderingDefinition
{
public:

	RenderingDefinition(rhi::ERenderingFlags renderingFlags, math::Vector2i renderAreaOffset, math::Vector2u renderAreaExtent)
	{
		m_rhiDefinition.renderingFlags		= renderingFlags;
		m_rhiDefinition.renderAreaOffset	= renderAreaOffset;
		m_rhiDefinition.renderAreaExtent	= renderAreaExtent;
	}

	void							AddColorRenderTarget(const RTDefinition& renderTarget)
	{
		m_rhiDefinition.colorRTs.push_back(GetRTDefRHI(renderTarget));
	}

	void							AddDepthRenderTarget(const RTDefinition& renderTarget)
	{
		m_rhiDefinition.depthRT = GetRTDefRHI(renderTarget);
	}

	void							AddStencilRenderTarget(const RTDefinition& renderTarget)
	{
		m_rhiDefinition.stencilRT = GetRTDefRHI(renderTarget);
	}

	const rhi::RenderingDefinition& GetRHI() const
	{
		return m_rhiDefinition;
	}

private:

	rhi::RHIRenderTargetDefinition				GetRTDefRHI(const RTDefinition& renderTarget) const
	{
		SPT_CHECK(!!renderTarget.textureView);

		rhi::RHIRenderTargetDefinition rhiDef;
		rhiDef.textureView			= renderTarget.textureView->GetRHI();

		if (renderTarget.resolveTextureView)
		{
			rhiDef.resolveTextureView = renderTarget.resolveTextureView->GetRHI();
		}

		rhiDef.loadOperation			= renderTarget.loadOperation;
		rhiDef.storeOperation			= renderTarget.storeOperation;
		rhiDef.resolveMode				= renderTarget.resolveMode;
		rhiDef.clearColor				= renderTarget.clearColor;

		return rhiDef;
	}

	rhi::RenderingDefinition		m_rhiDefinition;
};

}