#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

struct DirectionalLightShadowMasks
{
	DirectionalLightShadowMasks()
		: hasValidHistory(false)
	{ }

	lib::SharedPtr<rdr::TextureView> currentShadowMask;
	lib::SharedPtr<rdr::TextureView> historyShadowMask;
	
	Bool hasValidHistory;
};


struct ViewShadowMasksDataComponent
{
	lib::HashMap<RenderSceneEntity, DirectionalLightShadowMasks> directionalLightShadowMasks;
};


class DirectionalLightShadowMasksRenderStage : public RenderStage<DirectionalLightShadowMasksRenderStage, ERenderStage::DirectionalLightsShadowMasks>
{
protected:

	using Super = RenderStage<DirectionalLightShadowMasksRenderStage, ERenderStage::DirectionalLightsShadowMasks>;

public:

	DirectionalLightShadowMasksRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, RenderStageExecutionContext& stageContext);
};

} // spt::rsc