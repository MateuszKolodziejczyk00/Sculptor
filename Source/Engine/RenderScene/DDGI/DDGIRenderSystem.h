#pragma once

#include "RenderSystem.h"
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
};


class RENDER_SCENE_API DDGIRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:

	DDGIRenderSystem();

	// Begin RenderSystem overrides
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End RenderSystem overrides

private:

	void UpdateProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIUpdateParameters& updateParams) const;

	rg::RGTextureViewHandle TraceRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIUpdateParameters& updateParams) const;

	void RenderGlobalIllumination(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context) const;
	void RenderDebugProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const;
};

} // spt::rsc