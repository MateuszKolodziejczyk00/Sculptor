#pragma once

#include "SceneRenderSystem.h"
#include "DDGITypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc::ddgi
{

class DDGISceneSubsystem;
class DDGIVolume;
class DDGIScene;
struct DDGIConfig;


struct DDGIVolumeRelitParameters
{
	DDGIVolumeRelitParameters(rg::RenderGraphBuilder& graphBuilder, const DDGIRelitGPUParams& gpurelitParams, const DDGISceneSubsystem& ddgiSubsystem, const DDGIVolume& volume);

	const DDGIVolume& volume;

	lib::DynamicArray<rg::RGTextureViewHandle> probesIlluminanceTextureViews;
	lib::DynamicArray<rg::RGTextureViewHandle> probesHitDistanceTextureViews;
	rg::RGTextureViewHandle probesAverageLuminanceTextureView;

	rdr::BufferView relitParamsBuffer;
	rdr::BufferView ddgiVolumeParamsBuffer;

	Uint32 probesNumToUpdate;
	Uint32 raysNumPerProbe;

	math::Vector2u probeIlluminanceDataWithBorderRes;
	math::Vector2u probeHitDistanceDataWithBorderRes;

	lib::MTHandle<DDGISceneDS> ddgiSceneDS;

	EDDGIDebugMode::Type debugMode;
};


class RENDER_SCENE_API DDGIRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	DDGIRenderSystem();

	// Begin SceneRenderSystem overrides
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs) override;
	// End SceneRenderSystem overrides

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void RelitScene(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGISceneSubsystem& ddgiSubsystem, DDGIScene& scene) const;
	
	void InvalidateVolumes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGISceneSubsystem& ddgiSubsystem, DDGIScene& scene) const;

	void RelitVolume(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGISceneSubsystem& ddgiSubsystem, DDGIVolume& volume) const;
	void UpdateProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIVolumeRelitParameters& relitParams) const;

	void InvalidateOutOfBoundsProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, DDGIVolume& volume) const;

	rg::RGTextureViewHandle TraceRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIVolumeRelitParameters& relitParams) const;

	void RenderGlobalIllumination(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData) const;
	void RenderDebug(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const;

	DDGIRelitGPUParams CreateRelitParams(const DDGIVolume& volume, const DDGIConfig& config) const;
	Real32             ComputeRelitHysteresis(const DDGIVolume& volume, const DDGIConfig& config, Bool isFullVolumeRelit) const;
};

} // spt::rsc::ddgi