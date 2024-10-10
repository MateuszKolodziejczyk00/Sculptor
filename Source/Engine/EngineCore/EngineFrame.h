#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"
#include "Job.h"
#include "Event.h"
#include "Utility/Threading/Waitable.h"
#include "Engine.h"


namespace spt::engn
{

class FrameContext;

enum class EFrameState
{
	Updating,
	Rendering,
	GPU,

	NUM_TICKABLE_FRAMES = 2,
};


namespace EFrameStage
{

enum Type : Int32
{
	PreFrame = -1,

	Begin,
	
	// Render UI and request next stages jobs
	// e.g. update world that is visible on screen
	UI,
	AsyncWithUI = UI,
	
	ProcessViewsUpdate,

	ProcessViewsRendering,

	// Render UI
	RenderWindow,

	Finalize,

	Last = Finalize,

	Finished,
	NUM = Finished,

	First = Begin,

	UpdatingBegin = First,
	UpdatingEnd   = ProcessViewsUpdate,

	RenderingBegin = ProcessViewsRendering,
	RenderingEnd   = RenderWindow
};

} // EFrameStage


struct FrameDefinition
{
	FrameDefinition()
		: deltaTime(0.0f)
		, time(0.f)
		, frameIdx(0)
	{ }

	Real32 deltaTime;
	Real32 time;

	Uint64 frameIdx;
};


class ENGINE_CORE_API FrameContext : public std::enable_shared_from_this<FrameContext>
{
public:

	explicit FrameContext();
	virtual ~FrameContext();

	// Frame Flow =====================================================

	void BeginFrame(const FrameDefinition& definition, lib::SharedPtr<FrameContext> prevFrame);

	void WaitUpdateEnded();
	void WaitRenderingEnded();
	void WaitFrameFinished();

	void SetFrameFinishedOnGPUWaitable(lib::SharedPtr<lib::Waitable> waitable);

	void FlushPreviousFrames();

	// Frame Data =====================================================

	Real32 GetDeltaTime() const { return m_frameDefinition.deltaTime; }
	Real32 GetTime() const { return m_frameDefinition.time; }

	Uint64 GetFrameIdx() const { return m_frameDefinition.frameIdx; }

	EFrameState GetFrameState() const { return m_frameState; }

	void SetMaxFPS(Real32 fps);

	// Frame Stages ===================================================

	const js::Event& GetStageBeginEvent(EFrameStage::Type stage);
	const js::Event& GetStageFinishedEvent(EFrameStage::Type stage);

	void BeginAdvancingStages();

protected:

	virtual void DoStagesTransition(EFrameStage::Type prevStage, EFrameStage::Type nextStage);

private:

	FrameDefinition m_frameDefinition;

	EFrameState m_frameState;

	js::Event m_stageEndedEvents[EFrameStage::NUM];

	lib::SharedPtr<lib::Waitable> m_frameFinishedOnGPUWaitable;

	EFrameStage::Type m_currentStage;

	lib::SharedPtr<FrameContext> m_prevFrame;

	Real32 m_maxFPS;
};


using OnFrameTick = lib::MulticastDelegate<void(Real32 deltaTime)>;


class ENGINE_CORE_API EngineFramesManager
{
public:

	static EngineFramesManager& GetInstance();

	static void Shutdown();

	template<typename TFrameContextType>
	static lib::SharedPtr<TFrameContextType> CreateFrame();

	static void DispatchTickFrame(const FrameContext& context);

	static lib::DelegateHandle AddOnFrameTick(EFrameState frameState, OnFrameTick::Delegate&& delegate);
	static void RemoveOnFrameTick(EFrameState frameState, lib::DelegateHandle delegateHandle);

private:

	static constexpr Uint32 tickableFramesNum = (Uint32)EFrameState::NUM_TICKABLE_FRAMES;

	EngineFramesManager();

	OnFrameTick m_onFrameTick[tickableFramesNum];

	Uint64 m_frameCounter;

	lib::SharedPtr<FrameContext> m_lastFrame;
};

template<typename TFrameContextType>
static lib::SharedPtr<TFrameContextType> EngineFramesManager::CreateFrame()
{
	SPT_PROFILER_FUNCTION();

	EngineFramesManager& manager = GetInstance();

	const Uint64 frameIdx = manager.m_frameCounter++;

	lib::SharedPtr<TFrameContextType> frame = lib::MakeShared<TFrameContextType>();

	const Real32 deltaTime = Engine::Get().BeginFrame();
	const Real32 time      = Engine::Get().GetTime();

	FrameDefinition def;
	def.frameIdx  = frameIdx;
	def.deltaTime = deltaTime;
	def.time      = time;

	frame->BeginFrame(def, std::move(manager.m_lastFrame));

	manager.m_lastFrame = frame;

	return frame;
}

template<EFrameState frameState, typename TDerived>
class EngineTickable
{
public:

	EngineTickable()
	{
		m_onFrameTickHandle = EngineFramesManager::AddOnFrameTick(frameState,
																  OnFrameTick::Delegate::CreateRawMember(this, &EngineTickable::CallTick));
	}

	~EngineTickable()
	{
		EngineFramesManager::RemoveOnFrameTick(frameState, m_onFrameTickHandle);
	}

private:

	lib::DelegateHandle m_onFrameTickHandle;

	void CallTick(Real32 deltaTime)
	{
		reinterpret_cast<TDerived*>(this)->Tick(deltaTime);
	}
};

} // spt::engn