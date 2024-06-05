#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class DownsampleGeometryTexturesRenderStage : public RenderStage<DownsampleGeometryTexturesRenderStage, ERenderStage::DownsampleGeometryTextures>
{
public:

	DownsampleGeometryTexturesRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void PrepareResources(math::Vector2u renderingHalfRes);

	lib::SharedPtr<rdr::TextureView> m_normalsTextureHalfRes;
	lib::SharedPtr<rdr::TextureView> m_historyNormalsTextureHalfRes;

	lib::SharedPtr<rdr::TextureView> m_roughnessTextureHalfRes;
	lib::SharedPtr<rdr::TextureView> m_historyRoughnessTextureHalfRes;

	lib::SharedPtr<rdr::TextureView> m_specularColorTextureHalfRes;
	lib::SharedPtr<rdr::TextureView> m_historySpecularColorTextureHalfRes;
};

} // spt::rsc
