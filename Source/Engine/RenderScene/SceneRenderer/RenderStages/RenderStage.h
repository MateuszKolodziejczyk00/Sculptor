
#pragma once

#include "SculptorCoreTypes.h"
#include "View/ViewRenderingSpec.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;


struct RenderTargetFormatsDef
{
	lib::DynamicArray<rhi::EFragmentFormat> colorRTFormats;
	rhi::EFragmentFormat					depthRTFormat;
};


template<typename TRenderStageType, ERenderStage stage>
class RenderStage
{
public:

	RenderStage() = default;

	static constexpr ERenderStage GetStageEnum() { return stage; }

	void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings);

	RenderStageEntries& GetStageEntries(ViewRenderingSpec& viewSpec);
};


template<typename TRenderStageType, ERenderStage stage>
inline void RenderStage<TRenderStageType, stage>::Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	RenderStageEntries& viewStageEntries = GetStageEntries(viewSpec);

	RenderStageExecutionContext stageContext(settings, stage);

	viewStageEntries.BroadcastPreRenderStage(graphBuilder, renderScene, viewSpec, stageContext);

	reinterpret_cast<TRenderStageType*>(this)->OnRender(graphBuilder, renderScene, viewSpec, stageContext);
	
	viewStageEntries.BroadcastPostRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

template<typename TRenderStageType, ERenderStage stage>
inline RenderStageEntries& RenderStage<TRenderStageType, stage>::GetStageEntries(ViewRenderingSpec& viewSpec)
{
	return viewSpec.GetRenderStageEntries(stage);
}

} // spt::rsc
