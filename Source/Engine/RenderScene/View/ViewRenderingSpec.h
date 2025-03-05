#pragma once

#include "Delegates/MulticastDelegate.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "SculptorECS.h"
#include "RGResources/RGTrackedObject.h"
#include "RenderView.h"
#include "RGResources/RGResourceHandles.h"
#include "Utility/Blackboard.h"

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class RenderView;
class ViewRenderingSpec;
class DepthCullingDS;
class ViewShadingInputDS;
class ShadowMapsDS;



#define SPT_RENDER_STAGE_META_DATA_DEBUG (defined(SPT_DEBUG) || defined(SPT_DEVELOPMENT))


class RenderStageContextMetaDataHandle
{
public:

	RenderStageContextMetaDataHandle() = default;
	
	template<typename TDataType>
	RenderStageContextMetaDataHandle(const TDataType& data)
	{
		Bind(data);
	}

	template<typename TDataType>
	void Bind(const TDataType& data)
	{
#if SPT_RENDER_STAGE_META_DATA_DEBUG
		m_typeInfo = ecs::type_id<TDataType>();
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
		SPT_CHECK(m_typeInfo == ecs::type_id<TDataType>());
#endif // SPT_RENDER_STAGE_META_DATA_DEBUG

		SPT_CHECK(m_dataPtr != nullptr);
		return *reinterpret_cast<const TDataType*>(m_dataPtr);
	}

private:

	const void* m_dataPtr = nullptr;

#if SPT_RENDER_STAGE_META_DATA_DEBUG
	ecs::type_info m_typeInfo;
#endif // SPT_RENDER_STAGE_META_DATA_DEBUG
};


struct RenderStageExecutionContext
{
	explicit RenderStageExecutionContext(const SceneRendererSettings& inRendererSettings, ERenderStage inStage)
		: rendererSettings(inRendererSettings)
		, stage(inStage)
	{ }

	const SceneRendererSettings&     rendererSettings;
	const ERenderStage               stage;
};


class RenderStageEntries
{
public:

	using PreRenderStageDelegate	= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, ViewRenderingSpec& /*viewSpec*/, const RenderStageExecutionContext& /*context*/)>;
	using OnRenderStageDelegate		= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, ViewRenderingSpec& /*viewSpec*/, const RenderStageExecutionContext& /*context*/, RenderStageContextMetaDataHandle /*metaData*/)>;
	using PostRenderStageDelegate	= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, ViewRenderingSpec& /*viewSpec*/, const RenderStageExecutionContext& /*context*/)>;

	RenderStageEntries();

	PreRenderStageDelegate&		GetPreRenderStage();
	OnRenderStageDelegate&		GetOnRenderStage();
	PostRenderStageDelegate&	GetPostRenderStage();

	void BroadcastPreRenderStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);
	void BroadcastOnRenderStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData = RenderStageContextMetaDataHandle());
	void BroadcastPostRenderStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

private:

	PreRenderStageDelegate  m_preRenderStage;
	OnRenderStageDelegate	m_onRenderStage;
	PostRenderStageDelegate	m_postRenderStage;
};

struct RenderViewEntryContext
{
	RenderViewEntryContext()
		: texture(nullptr)
		, buffer(nullptr)
	{ }

	rg::RGTextureViewHandle texture;
	rg::RGBufferViewHandle buffer;
};


using RenderViewEntryDelegate = lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, ViewRenderingSpec& /*viewSpec*/, const RenderViewEntryContext& /*context*/)>;


namespace RenderViewEntryDelegates
{

/** Delegate for rendering objects onto view final texture after applying all post processes */
inline static const lib::HashedString RenderSceneDebugLayer = "SceneDebugLayer";

} // RenderViewEntryDelegates


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
	
	rg::RGTextureViewHandle skyViewLUT;

	GBuffer gBuffer;

	lib::MTHandle<ViewShadingInputDS> shadingInputDS;
	lib::MTHandle<ShadowMapsDS>       shadowMapsDS;

	rg::RGTextureViewHandle luminance;

	rg::RGTextureViewHandle output;
};


class ViewRenderingSpec : public rg::RGTrackedObject
{
public:

	ViewRenderingSpec();

	void Initialize(RenderView& renderView);
	
	RenderView& GetRenderView() const;
	ERenderStage GetSupportedStages() const;
	Bool SupportsStage(ERenderStage stage) const;

	math::Vector2u GetRenderingRes() const;
	math::Vector2u GetRenderingHalfRes() const;

	math::Vector2u GetOutputRes() const;

	ShadingViewContext&       GetShadingViewContext();
	const ShadingViewContext& GetShadingViewContext() const;

	template<typename TRenderStage>
	TRenderStage* GetRenderStage() const
	{
		return GetRenderView().GetRenderStage<TRenderStage>();
	}

	template<typename TRenderStage>
	TRenderStage& GetRenderStageChecked() const
	{
		return GetRenderView().GetRenderStageChecked<TRenderStage>();
	}

	const lib::Blackboard&	GetBlackboard() const;
	lib::Blackboard&		GetBlackboard();

	const RenderStageEntries&	GetRenderStageEntries(ERenderStage stage) const;
	RenderStageEntries&			GetRenderStageEntries(ERenderStage stage);

	RenderViewEntryDelegate&	GetRenderViewEntry(const lib::HashedString& name);

private:

	lib::HashMap<SizeType, RenderStageEntries> m_stagesEntries;

	lib::HashMap<lib::HashedString, RenderViewEntryDelegate> m_viewEntries;

	lib::Blackboard m_blackboard;

	RenderView* m_renderView;
};

} // spt::rsc