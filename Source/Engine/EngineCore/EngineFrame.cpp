#include "EngineFrame.h"
#include "Engine.h"
#include "JobSystem.h"

namespace spt::engn
{

namespace globals
{

static lib::UniquePtr<EngineFramesManager> g_engineFramesManager;

} // globals

//////////////////////////////////////////////////////////////////////////////////////////////////
// FrameContext ==================================================================================

FrameContext::FrameContext()
	: m_frameState(EFrameState::Simulation)
{ }

void FrameContext::AddFinalizeSimulationDelegate(OnFinalizeSimulation::Delegate delegate)
{
	m_finalizeSimulation.Add(std::move(delegate));
}

void FrameContext::AddOnSimulationFinishedDelegate(OnSimulationFinished::Delegate delegate)
{
	m_onSimulationFinished.Add(std::move(delegate));
}

void FrameContext::AddFinalizeRenderingDelegate(OnFinalizeRendering::Delegate delegate)
{
	m_finalizeRendering.Add(std::move(delegate));
}

void FrameContext::AddOnRenderingFinishedDelegate(OnRenderingFinished::Delegate delegate)
{
	m_onRenderingFinished.Add(std::move(delegate));
}

void FrameContext::AddFinalizeGPUDelegate(OnFinalizeGPU::Delegate delegate)
{
	m_finalizeGPU.Add(std::move(delegate));
}

void FrameContext::AddOnGPUFinishedDelegate(OnGPUFinished::Delegate delegate)
{
	m_onGPUFinished.Add(std::move(delegate));
}

void FrameContext::BeginFrame(const FrameDefinition& definition)
{
	m_frameDefinition = definition;
}

void FrameContext::SetFrameState(EFrameState state)
{
	m_frameState = state;
}

void FrameContext::FinalizeSimulation()
{
	SPT_PROFILER_FUNCTION();

	m_finalizeSimulation.ResetAndBroadcast(*this);
}

void FrameContext::EndSimulation()
{
	SPT_PROFILER_FUNCTION();

	m_onSimulationFinished.ResetAndBroadcast(*this);
}

void FrameContext::FinalizeRendering()
{
	SPT_PROFILER_FUNCTION();

	m_finalizeRendering.ResetAndBroadcast(*this);
}

void FrameContext::EndRendering()
{
	SPT_PROFILER_FUNCTION();

	m_onRenderingFinished.ResetAndBroadcast(*this);
}

void FrameContext::FinalizeGPU()
{
	SPT_PROFILER_FUNCTION();

	m_finalizeGPU.ResetAndBroadcast(*this);
}

void FrameContext::EndGPU()
{
	SPT_PROFILER_FUNCTION();

	m_onGPUFinished.ResetAndBroadcast(*this);
}

void FrameContext::EndFrame()
{
}

void FrameContext::Reset()
{
	m_finalizeSimulation.Reset();
	m_onSimulationFinished.Reset();
	m_finalizeRendering.Reset();
	m_onRenderingFinished.Reset();
	m_finalizeGPU.Reset();
	m_onGPUFinished.Reset();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

void EngineFramesManager::Initialize(FramesManagerInitializationInfo& info)
{
	globals::g_engineFramesManager.reset(new EngineFramesManager());
	globals::g_engineFramesManager->InitializeImpl(info);
}

void EngineFramesManager::Shutdown()
{
	GetInstance().ShutdownImpl();
}

EngineFramesManager& EngineFramesManager::GetInstance()
{
	SPT_CHECK(!!globals::g_engineFramesManager);
	return *globals::g_engineFramesManager;
}

FrameContext& EngineFramesManager::GetSimulationFrame()
{
	FrameContext* frame = GetInstance().m_simulationFrame;
	SPT_CHECK(!!frame);
	return *frame;
}

FrameContext& EngineFramesManager::GetRenderingFrame()
{
	FrameContext* frame = GetInstance().m_renderingFrame;
	SPT_CHECK(!!frame);
	return *frame;
}

FrameContext& EngineFramesManager::GetGPUFrame()
{
	FrameContext* frame = GetInstance().m_gpuFrame;
	SPT_CHECK(!!frame);
	return *frame;
}

void EngineFramesManager::ExecuteFrame()
{
	GetInstance().ExecuteFrameImpl();
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
	: m_frameCounter(0)
	, m_simulationFrame(nullptr)
	, m_renderingFrame(nullptr)
	, m_gpuFrame(nullptr)
{ }

void EngineFramesManager::InitializeImpl(FramesManagerInitializationInfo& info)
{
	m_preExecuteFrame			= std::move(info.preExecuteFrame);
	m_executeSimulationFrame	= std::move(info.executeSimulationFrame);
	m_executeRenderingFrame		= std::move(info.executeRenderingFrame);

	// We initialize dummy frames to avoid null pointers
	// This way we have valid frames through the whole engine lifetime
	FrameDefinition frameDef;
	frameDef.deltaTime			= Engine::Get().BeginFrame();
	frameDef.time				= Engine::Get().GetEngineTimer().GetTime();
	
	frameDef.frameIdx = 1;
	m_frames[1].BeginFrame(frameDef);
	m_gpuFrame = &m_frames[1];

	frameDef.frameIdx = 2;
	m_frames[2].BeginFrame(frameDef);
	m_renderingFrame = &m_frames[2];

	frameDef.frameIdx = 3;
	m_frames[0].BeginFrame(frameDef);
	m_simulationFrame = &m_frames[0];

	m_frameCounter = 4;
}

void EngineFramesManager::ShutdownImpl()
{
	SPT_PROFILER_FUNCTION();

	// We need to flush GPU frame before shutting down
	// to make sure that all resources can be safely released
	m_gpuFrame->FinalizeGPU();
	m_gpuFrame->EndFrame();
	
	m_simulationFrame->Reset();
	m_renderingFrame->Reset();
	m_gpuFrame->Reset();
}

void EngineFramesManager::ExecuteFrameImpl()
{
	SPT_PROFILER_FUNCTION();

	m_simulationFrame->SetFrameState(EFrameState::Simulation);
	m_renderingFrame->SetFrameState(EFrameState::Rendering);
	m_gpuFrame->SetFrameState(EFrameState::GPU);

	js::LaunchInline("Pre Execute Frame",
					 [this]
					 {
						 m_preExecuteFrame.ExecuteIfBound();
					 });
	
	js::LaunchInline("Execute Frame",
					 [this]
					 {
						 js::AddNested("Execute Simulation",
									   [this]
									   {
										   m_executeSimulationFrame.ExecuteIfBound(*m_simulationFrame);
										   m_simulationFrame->FinalizeSimulation();
									   },
									   js::EJobPriority::High);

						 js::AddNested("Execute Rendering",
									   [this]
									   {
										   m_executeRenderingFrame.ExecuteIfBound(*m_renderingFrame);
										   m_renderingFrame->FinalizeRendering();
									   },
									   js::EJobPriority::High);

						 js::AddNested("Execute GPU",
									   [this]
									   {
										   m_gpuFrame->FinalizeGPU();
									   },
									   js::EJobPriority::High);

					 });

	js::LaunchInline("End Frame",
					 [this]
					 {
						 js::AddNested("End Simulation",
									   [this]
									   {
										   m_simulationFrame->EndSimulation();
									   },
									   js::EJobPriority::High);

						 js::AddNested("End Rendering",
									   [this]
									   {
										   m_renderingFrame->EndRendering();
									   },
									   js::EJobPriority::High);

						 js::AddNested("End GPU",
									   [this]
									   {
										   m_gpuFrame->EndGPU();
										   m_gpuFrame->EndFrame();
									   },
									   js::EJobPriority::High);

					 });

	FrameDefinition frameDef;
	frameDef.deltaTime	= Engine::Get().BeginFrame();
	frameDef.time		= Engine::Get().GetEngineTimer().GetTime();
	frameDef.frameIdx	= m_frameCounter;

	m_gpuFrame			= m_renderingFrame;
	m_renderingFrame	= m_simulationFrame;
	m_simulationFrame	= &m_frames[m_frameCounter % parallelFramesNum];

	m_simulationFrame->BeginFrame(frameDef);

	++m_frameCounter;
}

} // spt::engn
