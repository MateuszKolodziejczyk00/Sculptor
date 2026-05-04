#pragma once

#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "SceneRenderingTypes.h"
#include "SceneRendererTypes.h"
#include "View/RenderView.h"
#include "RGResources/RGResourceHandles.h"
#include "Delegates/MulticastDelegate.h"
#include "Blackboard.h"

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;
class DepthCullingDS;
class ViewShadingInputDS;
class SharcCacheDS;
struct VolumetricFogParams;
class RenderStageBase;
class ViewRenderSystem;


namespace clouds
{
class CloudscapeProbesDS;
} // clouds


#define SPT_RENDER_STAGE_META_DATA_DEBUG (defined(SPT_DEBUG) || defined(SPT_DEVELOPMENT))


class RenderViewEntryContext
{
public:

	RenderViewEntryContext() = default;
	
	template<typename TDataType>
	RenderViewEntryContext(const TDataType& data)
	{
		Bind(data);
	}

	template<typename TDataType>
	void Bind(const TDataType& data)
	{
#if SPT_RENDER_STAGE_META_DATA_DEBUG
		m_typeInfo = lib::TypeInfo<TDataType>();
#endif // SPT_RENDER_STAGE_META_DATA_DEBUG

		m_dataPtr = &data;
	}

	void Reset()
	{
		m_dataPtr = nullptr;
	}

	Bool IsValid() const
	{
		return m_dataPtr != nullptr;
	}

	template<typename TDataType>
	const TDataType& Get() const
	{
#if SPT_RENDER_STAGE_META_DATA_DEBUG
		SPT_CHECK(m_typeInfo == lib::TypeInfo<TDataType>());
#endif // SPT_RENDER_STAGE_META_DATA_DEBUG

		SPT_CHECK(m_dataPtr != nullptr);
		return *reinterpret_cast<const TDataType*>(m_dataPtr);
	}

private:

	const void* m_dataPtr = nullptr;

#if SPT_RENDER_STAGE_META_DATA_DEBUG
	lib::RuntimeTypeInfo m_typeInfo;
#endif // SPT_RENDER_STAGE_META_DATA_DEBUG
};


struct RenderStageExecutionContext
{
	explicit RenderStageExecutionContext(const SceneRendererSettings& inRendererSettings, ERenderStage inStage)
		: rendererSettings(inRendererSettings)
		, stage(inStage)
	{ }

	const SceneRendererSettings& rendererSettings;
	const ERenderStage           stage;
};


using RenderViewEntryDelegate = lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, SceneRendererInterface& /* rendererInterface */, const RenderScene& /*scene*/, ViewRenderingSpec& /*viewSpec*/, const RenderViewEntryContext& /*context*/)>;


enum class ERenderViewEntry
{
	PreRendering,
	RenderAerialPerspective,
	RenderCloudsTransmittanceMap,
	RenderParticipatingMedia,
	RenderToShadowMap,
	CreateGlobalLightsDS,
	RenderGI,
	FillShadingDS,
	BuildLightsTiles,
	RenderVolumetricClouds,
	RenderVariableRateTexture,
	AntiAliasing,

	NUM
};


namespace RenderViewEntryDelegates
{

struct RenderAerialPerspectiveData
{
	const VolumetricFogParams* fogParams = nullptr;
};

struct FillShadingDSData
{
	lib::MTHandle<ViewShadingInputDS> ds;
};

struct RenderSceneDebugLayerData
{
	rg::RGTextureViewHandle texture;
};

} // RenderViewEntryDelegates


struct ShadingViewRenderingSystemsInfo
{
	// If true, all per-feature denoisers should be disabled and lighting should be composited with noisy signal that will be denoised later in the unified pass
	Bool useUnifiedDenoising = false;
};


struct ShadingViewResourcesUsageInfo
{
	Bool useHalfResRoughnessWithHistory = false;
	Bool useHalfResBaseColorWithHistory = false;

	Bool useLinearDepthHalfRes = false;
	Bool useLinearDepth        = false;

	Bool useRoughnessHistory             = false;
	Bool useBaseColorHistory             = false;
	Bool useOctahedronNormalsWithHistory = false;
};


struct ShadingViewContext
{
	rg::RGBufferViewHandle viewExposureData;

	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle depthHalfRes;

	rg::RGTextureViewHandle linearDepth;
	rg::RGTextureViewHandle linearDepthHalfRes;

	rg::RGTextureViewHandle hiZ;

	rg::RGTextureViewHandle historyDepth;
	rg::RGTextureViewHandle historyDepthHalfRes;

	lib::MTHandle<DepthCullingDS> depthCullingDS;

	rg::RGTextureViewHandle motion;
	rg::RGTextureViewHandle motionHalfRes;
	
	rg::RGTextureViewHandle historyBaseColor;

	rg::RGTextureViewHandle octahedronNormals;
	rg::RGTextureViewHandle historyOctahedronNormals;
	
	rg::RGTextureViewHandle baseColorHalfRes;
	rg::RGTextureViewHandle historyBaseColorHalfRes;

	rg::RGTextureViewHandle normalsHalfRes;
	rg::RGTextureViewHandle historyNormalsHalfRes;

	rg::RGTextureViewHandle roughnessHalfRes;
	rg::RGTextureViewHandle historyRoughnessHalfRes;

	rg::RGTextureViewHandle historyRoughness;

	rg::RGTextureViewHandle ambientOcclusion;

	rg::RGTextureViewHandle dirLightShadowMask;
	
	rg::RGTextureViewHandle skyViewLUT;
	rg::RGTextureViewHandle skyProbe;

	rg::RGTextureViewHandle aerialPerspective;

	rg::RGTextureViewHandle volumetricClouds;
	rg::RGTextureViewHandle volumetricCloudsDepth;

	lib::MTHandle<clouds::CloudscapeProbesDS> cloudscapeProbesDS;

	GBuffer gBuffer;

	lib::MTHandle<ViewShadingInputDS> shadingInputDS;

	lib::MTHandle<SharcCacheDS> sharcCacheDS;

	rg::RGTextureViewHandle luminance;

	rg::RGTextureViewHandle output;
};


class RenderViewPrivate : public RenderView
{
public:

	RenderViewPrivate(const RenderViewDefinition& definition);
	~RenderViewPrivate();

	lib::MemoryArena viewMemArena;

	const ERenderStage supportedStages = ERenderStage::None;

	Uint32 frameIdx = 0u;

	lib::SharedPtr<rdr::Buffer> viewExposureBuffer;

	ShadingRenderViewSettings shadingSettings;
	Bool settingsDirty = false;

	lib::StaticArray<RenderStageBase*, MaxRenderStagesNum>       renderStages;
	lib::StaticArray<ViewRenderSystem*, MaxViewRenderSystemsNum> renderSystems;
};


BEGIN_SHADER_STRUCT(ViewExposureData)
	SHADER_STRUCT_FIELD(Real32, exposureLastFrame)
	SHADER_STRUCT_FIELD(Real32, rcpExposureLastFrame)
	SHADER_STRUCT_FIELD(Real32, exposure)
	SHADER_STRUCT_FIELD(Real32, rcpExposure)
	SHADER_STRUCT_FIELD(Real32, EV100)
	SHADER_STRUCT_FIELD(Real32, averageLogLuminance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(RenderViewData)
	SHADER_STRUCT_FIELD(math::Vector2u, renderingResolution)
END_SHADER_STRUCT();


DS_BEGIN(RenderViewDS, rg::RGDescriptorSetState<RenderViewDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),               u_prevFrameSceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),               u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>),        u_cullingData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RenderViewData>),              u_viewRenderingParams)
	DS_BINDING(BINDING_TYPE(gfx::OptionalConstantBufferRefBinding<ViewExposureData>), u_viewExposure)
DS_END()


class ViewRenderingSpec
{
public:

	ViewRenderingSpec();

	void Initialize(rg::RenderGraphBuilder& graphBuilder, RenderViewPrivate& renderView);

	void CollectRenderViews(const RenderScene& renderScene, INOUT RenderViewsCollector& viewsCollector) const;

	void BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene);
	void EndFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene);

	void SetRenderingRes(const math::Vector2u& res);
	void SetJitter(const math::Vector2f& jitter);
	void ResetJitter();

	Uint32 GetFrameIdx() const { return m_renderView->frameIdx; }

	lib::MTHandle<RenderViewDS> GetRenderViewDS() const;

	template<typename TRenderSystem>
	TRenderSystem* GetRenderSystem() const
	{
		constexpr EViewRenderSystem type = TRenderSystem::type;
		const Uint32 typeIdx = GetViewRenderSystemIdx(type);
		return static_cast<TRenderSystem*>(m_renderView->renderSystems[typeIdx]);
	}

	template<typename TRenderSystem>
	TRenderSystem& GetRenderSystemChecked() const
	{
		TRenderSystem* renderSystem = GetRenderSystem<TRenderSystem>();
		SPT_CHECK(renderSystem != nullptr);
		return *renderSystem;
	}
	
	RenderView&  GetRenderView() const;
	ERenderStage GetSupportedStages() const;
	Bool         SupportsStage(ERenderStage stage) const;

	math::Vector2u GetRenderingRes() const;
	math::Vector3u GetRenderingRes3D() const;
	math::Vector2u GetRenderingHalfRes() const;

	math::Vector2u GetOutputRes() const;

	ShadingViewContext&       GetShadingViewContext();
	const ShadingViewContext& GetShadingViewContext() const;

	Bool                             AreSettingsDirty() const;
	const ShadingRenderViewSettings& GetShadingViewSettings() const;

	const lib::Blackboard&	GetBlackboard() const;
	lib::Blackboard&		GetBlackboard();

	RenderViewEntryDelegate&	GetRenderViewEntry(ERenderViewEntry entry);

private:

	lib::StaticArray<RenderViewEntryDelegate, static_cast<SizeType>(ERenderViewEntry::NUM)> m_viewEntries;

	lib::Blackboard m_blackboard;

	lib::MTHandle<RenderViewDS> m_renderViewDS;

	RenderViewPrivate* m_renderView;

	math::Vector2u m_renderingRes = math::Vector2u(0u, 0u);

	Bool m_settingsDirty = false;
};

} // spt::rsc
