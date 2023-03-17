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
	GPU
};

/**
 * These delegates are called when given state of frame is finished
 * Finalize delegates are called to finalize the state of frame and make sure that all data is ready for next state (e.g. here You can wait for GPU semaphore)
 * OnFinished delegates are called when We're sure that current state is finished and next state can be started
 */

using FinalizeSimulation	= lib::MulticastDelegate<void(FrameContext& context)>;
using OnSimulationFinished	= lib::MulticastDelegate<void(FrameContext& context)>;

using FinalizeRendering		= lib::MulticastDelegate<void(FrameContext& context)>;
using OnRenderingFinished	= lib::MulticastDelegate<void(FrameContext& context)>;

using FinalizeGPU			= lib::MulticastDelegate<void(FrameContext& context)>;
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

	// Frame Flow =====================================================

	void BeginFrame(const FrameDefinition& definition);
	void EndSimulation();
	void EndRendering();
	void EndGPU();
	void EndFrame();

	// Frame Delegates ================================================

	void AddFinalizeSimulationDelegate(FinalizeSimulation::Delegate delegate);
	void AddOnSimulationFinishedDelegate(OnSimulationFinished::Delegate delegate);

	void AddFinalizeRenderingDelegate(FinalizeRendering::Delegate delegate);
	void AddOnRenderingFinishedDelegate(OnRenderingFinished::Delegate delegate);

	void AddFinalizeGPUDelegate(FinalizeGPU::Delegate delegate);
	void AddOnGPUFinishedDelegate(OnGPUFinished::Delegate delegate);

	// Frame Data =====================================================

	Real32 GetDeltaTime() const { return m_frameDefinition.deltaTime; }

	Uint64 GetFrameIdx() const { return m_frameDefinition.frameIdx; }

private:

	FrameDefinition			m_frameDefinition;

	FinalizeSimulation		m_finalizeSimulation;
	OnSimulationFinished	m_onSimulationFinished;
	
	FinalizeRendering		m_finalizeRendering;
	OnRenderingFinished		m_onRenderingFinished;

	FinalizeGPU				m_finalizeGPU;
	OnGPUFinished			m_onGPUFinished;
};


using ExecuteSimulationFrame	= lib::Delegate<void(FrameContext& context)>;
using ExecuteRenderingFrame		= lib::Delegate<void(FrameContext& context)>;


struct FramesManagerInitializationInfo
{
	ExecuteSimulationFrame executeSimulationFrame;
	ExecuteRenderingFrame executeRenderingFrame;
};


class ENGINE_CORE_API EngineFramesManager
{
public:

	static void Initialize(FramesManagerInitializationInfo& info);

	static EngineFramesManager& GetInstance();

	static FrameContext& GetSimulationFrame();
	static FrameContext& GetRenderingFrame();
	static FrameContext& GetGPUFrame();

	static void ExecuteFrame();

private:

	static constexpr Uint32 parallelFramesNum = 3;

	EngineFramesManager();

	void InitializeImpl(FramesManagerInitializationInfo& info);

	void ExecuteFrameImpl();

	ExecuteSimulationFrame	m_executeSimulationFrame;
	ExecuteRenderingFrame	m_executeRenderingFrame;

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

} // spt::engn