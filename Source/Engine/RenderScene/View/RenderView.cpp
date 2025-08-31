#include "RenderView.h"
#include "RenderScene.h"
#include "ResourcesManager.h"
#include "SceneRenderer/RenderStages/RenderStage.h"
#include "Techniques/TemporalAA/TemporalAATypes.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderStagesRegistry ==========================================================================

RenderStagesRegistry::RenderStagesRegistry()
{
}

void RenderStagesRegistry::OnRenderStagesAdded(RenderView& renderView, ERenderStage newStages)
{
	Uint32 stagesToAdd = static_cast<Uint32>(newStages);
	while(stagesToAdd != 0)
	{
		const Uint32 stageIdx  = math::Utils::LowestSetBitIdx(stagesToAdd);
		const Uint32 stageFlag = 1u << stageIdx;

		lib::RemoveFlag(stagesToAdd, stageFlag);

		SPT_CHECK(!m_renderStages[stageIdx]);
		m_renderStages[stageIdx] = RenderStagesFactory::Get().CreateStage(static_cast<ERenderStage>(stageFlag));
		m_renderStages[stageIdx]->Initialize(renderView);
	}
}

void RenderStagesRegistry::OnRenderStagesRemoved(RenderView& renderView, ERenderStage removedStages)
{
	Uint32 stagesToRemove = static_cast<Uint32>(removedStages);
	while(stagesToRemove != 0)
	{
		const Uint32 stageIdx = math::Utils::LowestSetBitIdx(stagesToRemove);
		
		lib::RemoveFlag(stagesToRemove, 1u << stageIdx);

		SPT_CHECK(!!m_renderStages[stageIdx]);
		m_renderStages[stageIdx]->Deinitialize(renderView);
		m_renderStages[stageIdx].reset();
	}
}

RenderStageBase* RenderStagesRegistry::GetRenderStage(ERenderStage stage) const
{
	return m_renderStages[RenderStageToIndex(stage)].get();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderView ====================================================================================

RenderView::RenderView(RenderScene& renderScene)
	: m_supportedStages(ERenderStage::None)
	, m_renderingResolution(0u, 0u)
	, m_outputResolution(0u, 0u)
	, m_renderScene(renderScene)
	, m_renderedFrameIdx(0u)
{
	m_renderViewDS = rdr::ResourcesManager::CreateDescriptorSetState<RenderViewDS>(RENDERER_RESOURCE_NAME("RenderViewDS"));
}

RenderView::~RenderView()
{
}

void RenderView::SetRenderStages(ERenderStage stages)
{
	SPT_PROFILER_FUNCTION();

	const ERenderStage prevStages = m_supportedStages;

	m_supportedStages = stages;

	m_renderStages.OnRenderStagesAdded(*this, lib::Difference(m_supportedStages, prevStages));
	m_renderStages.OnRenderStagesRemoved(*this, lib::Difference(prevStages, m_supportedStages));
}

void RenderView::AddRenderStages(ERenderStage stagesToAdd)
{
	SPT_PROFILER_FUNCTION();

	const ERenderStage prevStages = m_supportedStages;

	lib::AddFlag(m_supportedStages, stagesToAdd);

	m_renderStages.OnRenderStagesAdded(*this, lib::Difference(m_supportedStages, prevStages));
}

void RenderView::RemoveRenderStages(ERenderStage stagesToRemove)
{
	SPT_PROFILER_FUNCTION();

	const ERenderStage prevStages = m_supportedStages;

	lib::RemoveFlag(m_supportedStages, stagesToRemove);

	m_renderStages.OnRenderStagesRemoved(*this, lib::Difference(prevStages, m_supportedStages));
}

ERenderStage RenderView::GetSupportedStages() const
{
	return m_supportedStages;
}

void RenderView::SetRenderingRes(const math::Vector2u& resolution)
{
	m_renderingResolution = resolution;
}

const math::Vector2u& RenderView::GetRenderingRes() const
{
	return m_renderingResolution;
}

math::Vector3u RenderView::GetRenderingRes3D() const
{
	return math::Vector3u(m_renderingResolution.x(), m_renderingResolution.y(), 1u);
}

void RenderView::SetOutputRes(const math::Vector2u& resolution)
{
	m_outputResolution = resolution;
	SetRenderingRes(m_outputResolution);
}

const math::Vector2u& RenderView::GetOutputRes() const
{
	return m_outputResolution;
}

math::Vector3u RenderView::GetOutputRes3D() const
{
	return math::Vector3u(m_outputResolution.x(), m_outputResolution.y(), 1u);
}

Uint32 RenderView::GetRenderedFrameIdx() const
{
	return m_renderedFrameIdx;
}

const math::Vector2u RenderView::GetRenderingHalfRes() const
{
	return math::Utils::DivideCeil(GetRenderingRes(), math::Vector2u(2u, 2u));
}

RenderScene& RenderView::GetRenderScene() const
{
	return m_renderScene;
}

lib::Blackboard& RenderView::GetBlackboard()
{
	return m_blackboard;
}

const lib::Blackboard& RenderView::GetBlackboard() const
{
	return m_blackboard;
}

const lib::MTHandle<RenderViewDS>& RenderView::GetRenderViewDS() const
{
	return m_renderViewDS;
}

void RenderView::SetExposureDataBuffer(lib::SharedPtr<rdr::Buffer> buffer)
{
	SPT_CHECK(m_renderViewDS.IsValid() && !m_renderViewDS->u_viewExposure.IsValid());
	m_renderViewDS->u_viewExposure = buffer->GetFullView();
}

void RenderView::ResetExposureDataBuffer()
{
	SPT_CHECK(m_renderViewDS->u_viewExposure.IsValid());
	m_renderViewDS->u_viewExposure.Reset();
}

lib::SharedPtr<rdr::BindableBufferView> RenderView::GetExposureDataBuffer() const
{
	return m_renderViewDS->u_viewExposure.GetBoundBuffer();
}

void RenderView::BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	++m_renderedFrameIdx;

	m_renderSystems.ForEachSystem([this](ViewRenderSystem& renderSystem) { renderSystem.PrepareRenderView(*this); });

	OnBeginRendering();

	m_renderSystems.ForEachSystem([&graphBuilder, &renderScene, &viewSpec](ViewRenderSystem& renderSystem) { renderSystem.BeginFrame(graphBuilder, renderScene, viewSpec); });

	m_renderStages.ForEachRenderStage([&renderScene, &viewSpec](RenderStageBase& stage) { stage.BeginFrame(renderScene, viewSpec); });
}

void RenderView::EndFrame(const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	m_renderStages.ForEachRenderStage([this, &renderScene](RenderStageBase& stage) { stage.EndFrame(renderScene, *this); });
}

const lib::DynamicArray<lib::SharedRef<ViewRenderSystem>>& RenderView::GetRenderSystems() const
{
	return m_renderSystems.GetRenderSystems();
}

void RenderView::UpdateRenderViewDS()
{
	SPT_PROFILER_FUNCTION();

	RenderViewData renderViewData;
	renderViewData.renderingResolution = GetRenderingRes();

	m_renderViewDS->u_prevFrameSceneView	= GetPrevFrameRenderingData();
	m_renderViewDS->u_sceneView				= GetViewRenderingData();
	m_renderViewDS->u_cullingData			= GetCullingData();
	m_renderViewDS->u_viewRenderingParams	= renderViewData;
}

void RenderView::CollectRenderViews(const RenderScene& renderScene, INOUT RenderViewsCollector& viewsCollector) const
{
	const lib::DynamicArray<lib::SharedRef<ViewRenderSystem>>& renderSystems = m_renderSystems.GetRenderSystems();
	for (const auto& renderSystem : renderSystems)
	{
		renderSystem->CollectRenderViews(renderScene, *this, INOUT viewsCollector);
	}
}

void RenderView::OnBeginRendering()
{
	SPT_PROFILER_FUNCTION();

	CachePrevFrameRenderingData();
	UpdateViewRenderingData(GetRenderingRes());
	UpdateCullingData();

	UpdateRenderViewDS();
}

void RenderView::InitializeRenderSystem(ViewRenderSystem& renderSystem)
{
	renderSystem.Initialize(*this);
}

void RenderView::DeinitializeRenderSystem(ViewRenderSystem& renderSystem)
{
	renderSystem.Deinitialize(*this);
}

} // spt::rsc
