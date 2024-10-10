#include "EngineFrame.h"
#include "Engine.h"
#include "JobSystem.h"

namespace spt::engn
{

SPT_DEFINE_LOG_CATEGORY(EngineFrame, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// FrameContext ==================================================================================

FrameContext::FrameContext()
	: m_frameState(EFrameState::Updating)
	, m_currentStage(EFrameStage::PreFrame)
	, m_maxFPS(-1.f)
{ }

FrameContext::~FrameContext()
{
	SPT_CHECK(m_currentStage == EFrameStage::Finished);
}

void FrameContext::BeginFrame(const FrameDefinition& definition, lib::SharedPtr<FrameContext> prevFrame)
{
	m_frameDefinition = definition;

	m_prevFrame = std::move(prevFrame);

	for (Int32 frameStageIdx = 0; frameStageIdx < static_cast<Int32>(EFrameStage::NUM); ++frameStageIdx)
	{
		m_stageEndedEvents[frameStageIdx] = js::CreateEvent("Advance Frame Stage",
															[this, frameStageIdx]()
															{
																const EFrameStage::Type currentStage = static_cast<EFrameStage::Type>(frameStageIdx);
																const EFrameStage::Type nextStage    = static_cast<EFrameStage::Type>(frameStageIdx + 1);
																
																DoStagesTransition(currentStage, nextStage);

																if (currentStage != EFrameStage::Last)
																{
																	m_stageEndedEvents[nextStage].Signal();
																}
															});
													   
	}
}

void FrameContext::WaitUpdateEnded()
{
	SPT_PROFILER_FUNCTION();

	m_stageEndedEvents[EFrameStage::UpdatingEnd].Wait();
}

void FrameContext::WaitRenderingEnded()
{
	SPT_PROFILER_FUNCTION();

	m_stageEndedEvents[EFrameStage::RenderingEnd].Wait();
}

void FrameContext::WaitFrameFinished()
{
	SPT_PROFILER_FUNCTION();

	m_stageEndedEvents[EFrameStage::Last].Wait();

	// Accessing this right now is thread-safe because all cpu work should be done by now (EFrameStage::Last is finished)
	if (m_frameFinishedOnGPUWaitable)
	{
		m_frameFinishedOnGPUWaitable->Wait();
		m_frameFinishedOnGPUWaitable.reset();
	}
}

void FrameContext::SetFrameFinishedOnGPUWaitable(lib::SharedPtr<lib::Waitable> waitable)
{
	SPT_CHECK(!m_frameFinishedOnGPUWaitable);
	m_frameFinishedOnGPUWaitable = std::move(waitable);
}

void FrameContext::FlushPreviousFrames()
{
	SPT_PROFILER_FUNCTION();

	if (m_prevFrame)
	{
		m_prevFrame->WaitFrameFinished();
		m_prevFrame.reset();
	}
}

void FrameContext::SetMaxFPS(Real32 fps)
{
	SPT_CHECK(m_currentStage < EFrameStage::UpdatingEnd); // Allowed only in updating stage

	m_maxFPS = fps;
}

const js::Event& FrameContext::GetStageBeginEvent(EFrameStage::Type stage)
{
	static js::Event invalid;
	const Int32 stageIdx = static_cast<Uint32>(stage);
	return stageIdx > 0 ? m_stageEndedEvents[stageIdx - 1] : invalid;
}

const js::Event& FrameContext::GetStageFinishedEvent(EFrameStage::Type stage)
{
	return m_stageEndedEvents[static_cast<Uint32>(stage)];
}

void FrameContext::BeginAdvancingStages()
{
	DoStagesTransition(EFrameStage::PreFrame, EFrameStage::Begin);
	m_stageEndedEvents[EFrameStage::Begin].Signal();
}

void FrameContext::DoStagesTransition(EFrameStage::Type prevStage, EFrameStage::Type nextStage)
{
	SPT_PROFILER_FUNCTION();

	if (nextStage == EFrameStage::UpdatingBegin)
	{
		if (m_prevFrame)
		{
			m_prevFrame->WaitUpdateEnded();
		}

		m_frameState = EFrameState::Updating;
	}

	if (prevStage == EFrameStage::UpdatingEnd)
	{
		// no-op
	}

	if (nextStage == EFrameStage::RenderingBegin)
	{
		if (m_prevFrame)
		{
			m_prevFrame->WaitRenderingEnded();
		}

		if (m_maxFPS > 0.f)
		{
			const Real32 currentTime          = GetEngineTimer().GetCurrentTimeSeconds();
			const Real32 currentFrameDuration = currentTime - m_frameDefinition.time;
			const Real32 targetFrameDuration  = 1.f / m_maxFPS;

			if (currentFrameDuration < targetFrameDuration)
			{
				const Real32 sleepTime = targetFrameDuration - currentFrameDuration;
				lib::CurrentThread::SleepFor(static_cast<Uint64>(sleepTime * 1000));
			}
		}

		m_frameState = EFrameState::Rendering;
	}
	
	if (prevStage == EFrameStage::RenderingEnd)
	{
		if (m_prevFrame)
		{
			m_prevFrame->WaitFrameFinished();
		}
		m_prevFrame.reset();

		m_frameState = EFrameState::GPU;
	}

	m_currentStage = nextStage;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

EngineFramesManager& EngineFramesManager::GetInstance()
{
	static EngineFramesManager manager;
	return manager;
}

void EngineFramesManager::Shutdown()
{
	EngineFramesManager& manager = GetInstance();

	manager.m_lastFrame->WaitFrameFinished();
	manager.m_lastFrame.reset();
}

void EngineFramesManager::DispatchTickFrame(const FrameContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(context.GetFrameState() != EFrameState::GPU);
	const Uint32 stateIdx = static_cast<Uint32>(context.GetFrameState());
	GetInstance().m_onFrameTick[stateIdx].Broadcast(context.GetDeltaTime());
}

lib::DelegateHandle EngineFramesManager::AddOnFrameTick(EFrameState frameState, OnFrameTick::Delegate&& delegate)
{
	SPT_CHECK(frameState != EFrameState::GPU);
	return GetInstance().m_onFrameTick[static_cast<Uint32>(frameState)].Add(std::move(delegate));
}

void EngineFramesManager::RemoveOnFrameTick(EFrameState frameState, lib::DelegateHandle delegateHandle)
{
	SPT_CHECK(frameState != EFrameState::GPU);
	GetInstance().m_onFrameTick[static_cast<Uint32>(frameState)].Unbind(delegateHandle);
}

EngineFramesManager::EngineFramesManager()
	: m_frameCounter(1)
{ }

} // spt::engn
