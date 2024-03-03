#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class HDRResolveRenderStage : public RenderStage<HDRResolveRenderStage, ERenderStage::HDRResolve>
{
protected:

	using Super = RenderStage<HDRResolveRenderStage, ERenderStage::HDRResolve>;

public:

	HDRResolveRenderStage();

	// Begin RenderStageBase overrides
	virtual void Initialize(RenderView& renderView) override;
	virtual void Deinitialize(RenderView& renderView) override;
	// End RenderStageBase overrides

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	lib::SharedPtr<rdr::Buffer> m_viewExposureBuffer;
};

} // spt::rsc