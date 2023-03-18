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
{ }

void FrameContext::BeginFrame(const FrameDefinition& definition)
{
	m_frameDefinition = definition;
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

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

void EngineFramesManager::Initialize(FramesManagerInitializationInfo& info)
{
	globals::g_engineFramesManager.reset(new EngineFramesManager());
	globals::g_engineFramesManager->InitializeImpl(info);
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

EngineFramesManager::EngineFramesManager()
	: m_frameCounter(0)
	, m_simulationFrame(nullptr)
	, m_renderingFrame(nullptr)
	, m_gpuFrame(nullptr)
{ }

void EngineFramesManager::InitializeImpl(FramesManagerInitializationInfo& info)
{
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

void EngineFramesManager::ExecuteFrameImpl()
{
	SPT_PROFILER_FUNCTION();
	
	js::Launch("Execute Frame",
			   [ this ]
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

			   },
			   js::EJobPriority::Default,
			   js::EJobFlags::Inline);

	js::Launch("End Frame",
			   [ this ]
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

			   },
			   js::EJobPriority::Default,
			   js::EJobFlags::Inline);

	FrameDefinition frameDef;
	frameDef.deltaTime		= Engine::Get().BeginFrame();
	frameDef.time			= Engine::Get().GetEngineTimer().GetTime();
	frameDef.frameIdx	= m_frameCounter;

	m_gpuFrame			= m_renderingFrame;
	m_renderingFrame	= m_simulationFrame;
	m_simulationFrame	= &m_frames[m_frameCounter % parallelFramesNum];

	m_simulationFrame->BeginFrame(frameDef);

	++m_frameCounter;
}

} // spt::engn
