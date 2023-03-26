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
public:

	DirectionalLightShadowMasks()
		: currentFrameShadowMaskIdx(0)
	{ }

	void AdvanceFrame()
	{
		currentFrameShadowMaskIdx = currentFrameShadowMaskIdx == 0 ? 1 : 0;
	}

	lib::SharedPtr<rdr::TextureView>& GetCurrentFrameShadowMask()
	{
		return shadowMasks[currentFrameShadowMaskIdx];
	}

	const lib::SharedPtr<rdr::TextureView>& GetCurrentFrameShadowMask() const
	{
		return shadowMasks[currentFrameShadowMaskIdx];
	}

	const lib::SharedPtr<rdr::TextureView>& GetPreviousFrameShadowMask() const
	{
		return shadowMasks[currentFrameShadowMaskIdx == 0 ? 1 : 0];
	}

private:

	lib::SharedPtr<rdr::TextureView> shadowMasks[2];
	Uint32 currentFrameShadowMaskIdx;
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