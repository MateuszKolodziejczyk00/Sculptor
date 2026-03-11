#include "ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "RenderStages/RenderStage.h"
#include "Utils/TransfersUtils.h"
#include "ViewRenderSystems/ViewRenderSystem.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderViewPrivate =============================================================================

RenderViewPrivate::RenderViewPrivate(const RenderViewDefinition& definition)
	: viewMemArena("RenderViewPrivate_ViewMemArena", 1024u * 1024u, 1024u * 1024u * 4u)
	, supportedStages(definition.renderStages)
{
	if (lib::HasAnyFlag(supportedStages, ERenderStage::DeferredLightingStages))
	{
		rhi::BufferDefinition bufferDef;
		bufferDef.size  = sizeof(rdr::HLSLStorage<ViewExposureData>);
		bufferDef.usage = lib::Flags(rhi::EBufferUsage::Uniform, rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferSrc, rhi::EBufferUsage::TransferDst);
		viewExposureBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("View Exposure Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);

		const Real32 defaultExposure = 1.f;
		rdr::FillBuffer(lib::Ref(viewExposureBuffer), 0u, bufferDef.size, *reinterpret_cast<const Uint32*>(&defaultExposure));
	}

	const RenderStagesAPI& stagesAPI = RenderStagesAPI::Get();

	std::fill(std::begin(renderStages), std::end(renderStages), nullptr);
	for (Uint32 stageIdx = 0; stageIdx < MaxRenderStagesNum; ++stageIdx)
	{
		const ERenderStage stage = static_cast<ERenderStage>(1u << stageIdx);
		if (lib::HasAnyFlag(supportedStages, stage))
		{
			renderStages[stageIdx] = stagesAPI.CallConstructor(stage, viewMemArena);
			stagesAPI.CallInitializeFunc(stage, *renderStages[stageIdx], *this);
		}
	}

	const ViewRenderSystemsAPI& systemsAPI = ViewRenderSystemsAPI::Get();

	std::fill(std::begin(renderSystems), std::end(renderSystems), nullptr);
	for (Uint32 systemIdx = 0; systemIdx < MaxViewRenderSystemsNum; ++systemIdx)
	{
		const EViewRenderSystem systemType = static_cast<EViewRenderSystem>(1u << systemIdx);
		if (lib::HasAnyFlag(definition.renderSystems, systemType))
		{
			renderSystems[systemIdx] = systemsAPI.CallConstructor(systemType, viewMemArena, *this);
			systemsAPI.CallInitialize(systemType, *renderSystems[systemIdx], *this);
		}
	}

}

RenderViewPrivate::~RenderViewPrivate()
{
	const RenderStagesAPI& stagesAPI = RenderStagesAPI::Get();

	for (Uint32 stageIdx = 0; stageIdx < MaxRenderStagesNum; ++stageIdx)
	{
		if (renderStages[stageIdx])
		{
			const ERenderStage stage = static_cast<ERenderStage>(1u << stageIdx);

			stagesAPI.CallDeinitializeFunc(stage, *renderStages[stageIdx], *this);
			stagesAPI.CallDestructor(stage, *renderStages[stageIdx]);
		}
	}

	const ViewRenderSystemsAPI& systemsAPI = ViewRenderSystemsAPI::Get();
	for (Uint32 systemIdx = 0; systemIdx < MaxViewRenderSystemsNum; ++systemIdx)
	{
		if (renderSystems[systemIdx])
		{
			const EViewRenderSystem systemType = static_cast<EViewRenderSystem>(1u << systemIdx);

			systemsAPI.CallDeinitialize(systemType, *renderSystems[systemIdx], *this);
			systemsAPI.CallDestructor(systemType, *renderSystems[systemIdx]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ViewRenderingSpec =============================================================================

ViewRenderingSpec::ViewRenderingSpec()
	: m_renderView(nullptr)
{ }

void ViewRenderingSpec::Initialize(rg::RenderGraphBuilder& graphBuilder, RenderViewPrivate& renderView)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!m_renderView);
	
	m_renderView = &renderView;

	m_settingsDirty = m_renderView->settingsDirty;
	m_renderView->settingsDirty = false;

	const Bool isShadingView = lib::HasAnyFlag(renderView.supportedStages, ERenderStage::DeferredLightingStages);
	if (isShadingView)
	{
		m_blackboard.Create<ShadingViewResourcesUsageInfo>();
		m_blackboard.Create<ShadingViewRenderingSystemsInfo>();

		ShadingViewContext& shadingContext = m_blackboard.Create<ShadingViewContext>();
		shadingContext.viewExposureData = graphBuilder.AcquireExternalBufferView(renderView.viewExposureBuffer->GetFullView());
	}
}

void ViewRenderingSpec::CollectRenderViews(const RenderScene& renderScene, INOUT RenderViewsCollector& viewsCollector) const
{
	const ViewRenderSystemsAPI& systemsAPI = ViewRenderSystemsAPI::Get();
	for (Uint32 systemIdx = 0; systemIdx < m_renderView->renderSystems.size(); ++systemIdx)
	{
		if (ViewRenderSystem* systemInstance = m_renderView->renderSystems[systemIdx])
		{
			const EViewRenderSystem systemType = static_cast<EViewRenderSystem>(1u << systemIdx);
			systemsAPI.CallCollectRenderViews(systemType, *systemInstance, renderScene, *m_renderView, INOUT viewsCollector);
		}
	}
}

void ViewRenderingSpec::BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene)
{
	SPT_PROFILER_FUNCTION();

	m_renderView->frameIdx++;

	const ViewRenderSystemsAPI& systemsAPI = ViewRenderSystemsAPI::Get();
	for (Uint32 systemIdx = 0; systemIdx < m_renderView->renderSystems.size(); ++systemIdx)
	{
		if (ViewRenderSystem* systemInstance = m_renderView->renderSystems[systemIdx])
		{
			const EViewRenderSystem systemType = static_cast<EViewRenderSystem>(1u << systemIdx);
			systemsAPI.CallPrepareRenderView(systemType, *systemInstance, *this);
		}
	}

	if (m_renderingRes == math::Vector2u(0u, 0u))
	{
		m_renderingRes = m_renderView->GetOutputRes();
	}

	m_renderView->BeginFrame(m_renderingRes);

	RenderViewData renderViewData;
	renderViewData.renderingResolution = GetRenderingRes();

	m_renderViewDS = graphBuilder.CreateDescriptorSet<RenderViewDS>(RENDERER_RESOURCE_NAME("RenderViewDS"));
	m_renderViewDS->u_prevFrameSceneView  = m_renderView->GetPrevFrameRenderingData();
	m_renderViewDS->u_sceneView           = m_renderView->GetViewRenderingData();
	m_renderViewDS->u_cullingData         = m_renderView->GetCullingData();
	m_renderViewDS->u_viewRenderingParams = renderViewData;

	if (m_renderView->viewExposureBuffer)
	{
		m_renderViewDS->u_viewExposure = m_renderView->viewExposureBuffer->GetFullView();
	}

	for (Uint32 systemIdx = 0; systemIdx < m_renderView->renderSystems.size(); ++systemIdx)
	{
		if (ViewRenderSystem* systemInstance = m_renderView->renderSystems[systemIdx])
		{
			const EViewRenderSystem systemType = static_cast<EViewRenderSystem>(1u << systemIdx);
			systemsAPI.CallBeginFrame(systemType, *systemInstance, graphBuilder, scene, *this);
		}
	}

	const RenderStagesAPI& stagesAPI = RenderStagesAPI::Get();
	for (Uint32 stageIdx = 0; stageIdx < m_renderView->renderStages.size(); ++stageIdx)
	{
		if (RenderStageBase* stageInstance = m_renderView->renderStages[stageIdx])
		{
			const ERenderStage stage = static_cast<ERenderStage>(1u << stageIdx);
			stagesAPI.CallBeginFrameFunc(stage, *stageInstance, scene, *this);
		}
	}
}

void ViewRenderingSpec::EndFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene)
{
	SPT_PROFILER_FUNCTION();

	const RenderStagesAPI& stagesAPI = RenderStagesAPI::Get();

	for (Uint32 stageIdx = 0; stageIdx < m_renderView->renderStages.size(); ++stageIdx)
	{
		if (RenderStageBase* stageInstance = m_renderView->renderStages[stageIdx])
		{
			const ERenderStage stage = static_cast<ERenderStage>(1u << stageIdx);
			stagesAPI.CallEndFrameFunc(stage, *stageInstance, scene, *m_renderView);
		}
	}
}

void ViewRenderingSpec::SetRenderingRes(const math::Vector2u& res)
{
	m_renderingRes = res;
}

void ViewRenderingSpec::SetJitter(const math::Vector2f& jitter)
{
	m_renderView->SetJitter(jitter);
}

void ViewRenderingSpec::ResetJitter()
{
	m_renderView->ResetJitter();
}

lib::MTHandle<RenderViewDS> ViewRenderingSpec::GetRenderViewDS() const
{
	return m_renderViewDS;
}

RenderView& ViewRenderingSpec::GetRenderView() const
{
	SPT_CHECK(!!m_renderView);
	return *m_renderView;
}

ERenderStage ViewRenderingSpec::GetSupportedStages() const
{
	SPT_CHECK(!!m_renderView);
	return m_renderView->supportedStages;
}

Bool ViewRenderingSpec::SupportsStage(ERenderStage stage) const
{
	SPT_CHECK(!!m_renderView);
	return lib::HasAnyFlag(m_renderView->supportedStages, stage);
}

math::Vector2u ViewRenderingSpec::GetRenderingRes() const
{
	SPT_CHECK(!!m_renderView);
	return m_renderingRes;
}

math::Vector3u ViewRenderingSpec::GetRenderingRes3D() const
{
	return math::Vector3u(GetRenderingRes().x(), GetRenderingRes().y(), 1u);
}

math::Vector2u ViewRenderingSpec::GetRenderingHalfRes() const
{
	SPT_CHECK(!!m_renderView);
	return math::Utils::DivideCeil(GetRenderingRes(), math::Vector2u(2u, 2u));
}

math::Vector2u ViewRenderingSpec::GetOutputRes() const
{
	SPT_CHECK(!!m_renderView);
	return m_renderView->GetOutputRes();
}

ShadingViewContext& ViewRenderingSpec::GetShadingViewContext()
{
	return m_blackboard.Get<ShadingViewContext>();
}

const ShadingViewContext& ViewRenderingSpec::GetShadingViewContext() const
{
	return m_blackboard.Get<ShadingViewContext>();
}

Bool ViewRenderingSpec::AreSettingsDirty() const
{
	return m_settingsDirty;
}

const ShadingRenderViewSettings& ViewRenderingSpec::GetShadingViewSettings() const
{
	return m_renderView->shadingSettings;
}

const lib::Blackboard& ViewRenderingSpec::GetBlackboard() const
{
	return m_blackboard;
}

lib::Blackboard& ViewRenderingSpec::GetBlackboard()
{
	return m_blackboard;
}

RenderViewEntryDelegate& ViewRenderingSpec::GetRenderViewEntry(ERenderViewEntry entry)
{
	return m_viewEntries[static_cast<SizeType>(entry)];
}

} // spt::rsc
