#pragma once

#include "SceneRenderSystem.h"
#include "DDGITypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

class DDGISceneSubsystem;


struct DDGIUpdateParameters
{
	DDGIUpdateParameters(const DDGIUpdateProbesGPUParams& gpuUpdateParams, const DDGISceneSubsystem& ddgiSubsystem);

	lib::SharedPtr<rdr::TextureView> probesIlluminanceTextureView;
	lib::SharedPtr<rdr::TextureView> probesHitDistanceTextureView;

	rdr::BufferView updateProbesParamsBuffer;
	rdr::BufferView ddgiParamsBuffer;

	Uint32 probesNumToUpdate;
	Uint32 raysNumPerProbe;

	math::Vector2u probeIlluminanceDataWithBorderRes;
	math::Vector2u probeHitDistanceDataWithBorderRes;

	EDDGIDebugMode::Type debugMode;
};


class RENDER_SCENE_API DDGIRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	DDGIRenderSystem();

	// Begin SceneRenderSystem overrides
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpec) override;
	// End SceneRenderSystem overrides

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void UpdateProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIUpdateParameters& updateParams) const;

	rg::RGTextureViewHandle TraceRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIUpdateParameters& updateParams) const;

	void RenderGlobalIllumination(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context) const;
	void RenderDebug(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const;
};

} // spt::rsc