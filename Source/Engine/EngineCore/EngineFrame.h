#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::engn
{

class FrameContext;


enum class EFrameState
{
	Simulation,
	Rendering,
	GPU,
	NUM_TICKABLE_FRAMES = GPU,
	NUM_PARALLEL_FRAMES
};


/**
 * These delegates are called when given state of frame is finished
 * Finalize delegates are called to finalize the state of frame and make sure that all data is ready for next state (e.g. here You can wait for GPU semaphore)
 * OnFinished delegates are called when We're sure that current state is finished and next state can be started
 */

using OnFinalizeSimulation	= lib::MulticastDelegate<void(FrameContext& context)>;
using OnSimulationFinished	= lib::MulticastDelegate<void(FrameContext& context)>;

using OnFinalizeRendering	= lib::MulticastDelegate<void(FrameContext& context)>;
using OnRenderingFinished	= lib::MulticastDelegate<void(FrameContext& context)>;

using OnFinalizeGPU			= lib::MulticastDelegate<void(FrameContext& context)>;
using OnGPUFinished			= lib::MulticastDelegate<void(FrameContext& context)>;


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


class ENGINE_CORE_API FrameContext
{
public:

	FrameContext();

	// Frame Delegates ================================================

	void AddFinalizeSimulationDelegate(OnFinalizeSimulation::Delegate delegate);
	void AddOnSimulationFinishedDelegate(OnSimulationFinished::Delegate delegate);

	void AddFinalizeRenderingDelegate(OnFinalizeRendering::Delegate delegate);
	void AddOnRenderingFinishedDelegate(OnRenderingFinished::Delegate delegate);

	void AddFinalizeGPUDelegate(OnFinalizeGPU::Delegate delegate);
	void AddOnGPUFinishedDelegate(OnGPUFinished::Delegate delegate);

	// Frame Data =====================================================

	Real32 GetDeltaTime() const { return m_frameDefinition.deltaTime; }
	Real32 GetTime() const { return m_frameDefinition.time; }

	Uint64 GetFrameIdx() const { return m_frameDefinition.frameIdx; }

	EFrameState GetFrameState() const { return m_frameState; }

private:

	// Frame Flow =====================================================

	void BeginFrame(const FrameDefinition& definition);

	void SetFrameState(EFrameState state);

	void FinalizeSimulation();
	void EndSimulation();

	void FinalizeRendering();
	void EndRendering();

	void FinalizeGPU();
	void EndGPU();

	void EndFrame();

	void Reset();

	FrameDefinition			m_frameDefinition;

	OnFinalizeSimulation	m_finalizeSimulation;
	OnSimulationFinished	m_onSimulationFinished;
	
	OnFinalizeRendering		m_finalizeRendering;
	OnRenderingFinished		m_onRenderingFinished;

	OnFinalizeGPU			m_finalizeGPU;
	OnGPUFinished			m_onGPUFinished;

	EFrameState				m_frameState;

	friend class EngineFramesManager;
};


using PreExecuteFrame			= lib::Delegate<void()>;
using ExecuteSimulationFrame	= lib::Delegate<void(FrameContext& context)>;
using ExecuteRenderingFrame		= lib::Delegate<void(FrameContext& context)>;


using OnFrameTick = lib::MulticastDelegate<void(Real32 deltaTime)>;


struct FramesManagerInitializationInfo
{
	PreExecuteFrame			preExecuteFrame;
	ExecuteSimulationFrame	executeSimulationFrame;
	ExecuteRenderingFrame	executeRenderingFrame;
};


class ENGINE_CORE_API EngineFramesManager
{
public:

	static void Initialize(FramesManagerInitializationInfo& info);

	static void Shutdown();

	static EngineFramesManager& GetInstance();

	static FrameContext& GetSimulationFrame();
	static FrameContext& GetRenderingFrame();
	static FrameContext& GetGPUFrame();

	static void ExecuteFrame();

	static void DispatchTickFrame(const FrameContext& context);

	static lib::DelegateHandle AddOnFrameTick(EFrameState frameState, OnFrameTick::Delegate&& delegate);
	static void RemoveOnFrameTick(EFrameState frameState, lib::DelegateHandle delegateHandle);

private:

	static constexpr Uint32 parallelFramesNum = (Uint32)EFrameState::NUM_PARALLEL_FRAMES;
	static constexpr Uint32 tickableFramesNum = (Uint32)EFrameState::NUM_TICKABLE_FRAMES;

	EngineFramesManager();

	void InitializeImpl(FramesManagerInitializationInfo& info);

	void ShutdownImpl();

	void ExecuteFrameImpl();
	
	PreExecuteFrame			m_preExecuteFrame;

	ExecuteSimulationFrame	m_executeSimulationFrame;
	ExecuteRenderingFrame	m_executeRenderingFrame;

	OnFrameTick m_onFrameTick[tickableFramesNum];

	Uint64 m_frameCounter;

	FrameContext* m_simulationFrame;
	FrameContext* m_renderingFrame;
	FrameContext* m_gpuFrame;

	FrameContext m_frames[parallelFramesNum];
};


inline FrameContext& GetSimulationFrame()
{
	return EngineFramesManager::GetSimulationFrame();
}


inline FrameContext& GetRenderingFrame()
{
	return EngineFramesManager::GetRenderingFrame();
}


inline FrameContext& GetGPUFrame()
{
	return EngineFramesManager::GetGPUFrame();
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