#pragma once

#include "DDGI/DDGIScene.h"
#include "SceneRenderSystem.h"
#include "DDGITypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc::ddgi
{

class DDGISceneSubsystem;
class DDGIVolume;
class DDGIRelitZone;
class DDGIScene;


struct DDGILODConfig
{
	math::Vector3u volumeResolution    = math::Vector3u::Constant(12u);
	math::Vector3u relitZoneResolution = math::Vector3u::Constant(12u);
	math::Vector3f probesSpacing       = math::Vector3f::Constant(1.f);

	Real32 relitPriority = 1.f;

	Real32 forwardAlignment = 0.5f;
	Real32 heightAlignment  = 0.2f;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("VolumeResolution", volumeResolution);
		serializer.Serialize("RelitZoneResolution", relitZoneResolution);
		serializer.Serialize("ProbesSpacing", probesSpacing);
		serializer.Serialize("RelitPriority", relitPriority);
		serializer.Serialize("ForwardAlignment", forwardAlignment);
		serializer.Serialize("HeightAlignment", heightAlignment);
	}
};


struct DDGIConfig
{
	Uint32 relitRaysPerProbe    = 400u;

	Real32 defaultRelitHysteresis = 0.97f;

	Real32 minHysteresis         = 0.3f;
	Real32 maxHysteresis         = 0.995f;

	Uint32 relitVolumesBudget = 3u;
	
	math::Vector2u probeIlluminanceDataRes = math::Vector2u::Constant(6u);
	math::Vector2u probeHitDistanceDataRes = math::Vector2u::Constant(16u);

	lib::DynamicArray<DDGILODConfig> lodsConfigs = { DDGILODConfig{} };

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("RelitRaysPerProbe", relitRaysPerProbe);
		serializer.Serialize("DefaultRelitHysteresis", defaultRelitHysteresis);
		serializer.Serialize("MinHysteresis", minHysteresis);
		serializer.Serialize("MaxHysteresis", maxHysteresis);
		serializer.Serialize("RelitVolumesBudget", relitVolumesBudget);
		serializer.Serialize("ProbeIlluminanceDataRes", probeIlluminanceDataRes);
		serializer.Serialize("ProbeHitDistanceDataRes", probeHitDistanceDataRes);
		serializer.Serialize("LODsConfigs", lodsConfigs);
	}
};


struct DDGIVolumeRelitParameters
{
	DDGIVolumeRelitParameters(rg::RenderGraphBuilder& graphBuilder, const DDGIRelitGPUParams& relitParams, const DDGIScene& ddgiScene, const DDGIVolume& inVolume);

	const DDGIVolume& volume;

	lib::DynamicArray<rg::RGTextureViewHandle> probesIlluminanceTextureViews;
	lib::DynamicArray<rg::RGTextureViewHandle> probesHitDistanceTextureViews;
	rg::RGTextureViewHandle probesAverageLuminanceTextureView;

	lib::SharedPtr<rdr::BindableBufferView> relitParamsBuffer;
	lib::SharedPtr<rdr::BindableBufferView> ddgiVolumeParamsBuffer;

	Uint32 probesNumToUpdate;
	Uint32 raysNumPerProbe;

	math::Vector2u probeIlluminanceDataWithBorderRes;
	math::Vector2u probeHitDistanceDataWithBorderRes;

	lib::MTHandle<DDGISceneDS> ddgiSceneDS;
};


class RENDER_SCENE_API DDGIRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	explicit DDGIRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) override;
	// End SceneRenderSystem overrides

	const lib::MTHandle<DDGISceneDS>& GetDDGISceneDS() const { return m_ddgiScene.GetDDGIDS(); }

	Bool IsDDGIEnabled() const;

private:

	struct RelitSettings
	{
		Bool reset = false;
	};

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void RelitScene(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RelitSettings& settings);
	
	void InvalidateVolumes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RelitSettings& settings) const;

	void RelitZone(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, DDGIRelitZone& zone) const;
	void UpdateProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIVolumeRelitParameters& relitParams) const;

	void InvalidateOutOfBoundsProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, DDGIVolume& volume, const RelitSettings& settings) const;

	rg::RGTextureViewHandle TraceRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIVolumeRelitParameters& relitParams) const;

	void RenderGlobalIllumination(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData);
	void RenderDebug(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const;

	DDGIRelitGPUParams CreateRelitParams(const DDGIRelitZone& zone, const DDGIConfig& config) const;
	Real32             ComputeRelitHysteresis(const DDGIRelitZone& zone, const DDGIConfig& config) const;

	DDGIConfig m_config;

	DDGIScene m_ddgiScene;
};

} // spt::rsc::ddgi
