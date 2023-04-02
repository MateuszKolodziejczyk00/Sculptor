#include "SculptorEdApplication.h"
#include "Renderer.h"
#include "ResourcesManager.h"
#include "Types/Semaphore.h"
#include "Types/Texture.h"
#include "Types/RenderContext.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "CommandsRecorder/RenderingDefinition.h"
#include "UIContextManager.h"
#include "Profiler.h"
#include "GPUDiagnose/Diagnose.h"
#include "Engine.h"
#include "ImGui/SculptorImGui.h"
#include "Types/Sampler.h"
#include "JobSystem/JobSystem.h"
#include "UIElements/ApplicationUI.h"
#include "UI/SandboxUIView.h"
#include "Renderer/SandboxRenderer.h"
#include "UIUtils.h"
#include "ProfilerUIView.h"
#include "RenderGraphManager.h"
#include "Paths.h"
#include "RenderingDataRegistry.h"


namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(AppLog, true)


SculptorEdApplication::SculptorEdApplication()
	: isSwapchainValid(true)
{ }

void SculptorEdApplication::OnInit(int argc, char** argv)
{
	Super::OnInit(argc, argv);

	engn::EngineInitializationParams engineInitializationParams;
	engineInitializationParams.cmdLineArgsNum = argc;
	engineInitializationParams.cmdLineArgs = argv;
	engn::Engine::Get().Initialize(engineInitializationParams);

	const SizeType threadsNum = static_cast<SizeType>(std::thread::hardware_concurrency());

	js::JobSystemInitializationParams jobSystemParams;
	jobSystemParams.workerThreadsNum = threadsNum - 1;
	js::JobSystem::Initialize(jobSystemParams);

	prf::Profiler::Get().Initialize();

	rdr::Renderer::Initialize();

	rhi::RHIWindowInitializationInfo windowInitInfo;
	windowInitInfo.framebufferSize = math::Vector2u(1920, 1080);
	windowInitInfo.enableVSync = false;
	m_window = rdr::ResourcesManager::CreateWindow("SculptorEd", windowInitInfo);

	rdr::Renderer::PostCreatedWindow();
}

void SculptorEdApplication::OnRun()
{
	Super::OnRun();

	IMGUI_CHECKVERSION();
	uiContext = ImGui::CreateContext();

	ImGuiIO& imGuiIO = ImGui::GetIO();
	imGuiIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	imGuiIO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	imGuiIO.IniFilename = engn::Paths::GetImGuiConfigPath().data();

	m_window->InitializeUI(uiContext);

	ui::UIContextManager::SetGlobalContext(uiContext);

	rdr::UIBackend::Initialize(uiContext, lib::Ref(m_window));

	engn::FramesManagerInitializationInfo framesManagerInitInfo;
	framesManagerInitInfo.executeSimulationFrame.BindRawMember(this, &SculptorEdApplication::ExecuteSimulationFrame);
	framesManagerInitInfo.executeRenderingFrame.BindRawMember(this, &SculptorEdApplication::ExecuteRenderingFrame);
	engn::EngineFramesManager::Initialize(framesManagerInitInfo);

	rdr::Renderer::IncrementFrameReleaseSemaphore(engn::GetGPUFrame().GetFrameIdx());

	ImGui::SetCurrentContext(uiContext.GetHandle());

	{
		lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

		recorder->InitializeUIFonts();

		const lib::SharedRef<rdr::RenderContext> renderingContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("InitUIContext"), rhi::ContextDefinition());

		rdr::CommandsRecordingInfo recordingInfo;
		recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("InitializeUICommandBuffer");
		recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		recorder->RecordCommands(renderingContext, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

		lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
		rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
		submitBatch.recordedCommands.emplace_back(std::move(recorder));

		rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

		rdr::Renderer::WaitIdle();

		rdr::UIBackend::DestroyFontsTemporaryObjects();
	}

	m_renderer = std::make_unique<SandboxRenderer>(m_window);

	scui::ViewDefinition sandboxViewDef;
	sandboxViewDef.name = "SandboxView";
	lib::SharedRef<SandboxUIView> view = scui::ApplicationUI::OpenView<SandboxUIView>(sandboxViewDef, *m_renderer);

	scui::ViewDefinition profilerViewDef;
	profilerViewDef.name = "ProfilerView";
	view->AddChild(lib::MakeShared<prf::ProfilerUIView>(profilerViewDef));

	while (true)
	{
		SPT_PROFILER_FRAME("EditorFrame");

		m_window->Update(engn::GetSimulationFrame().GetDeltaTime());

		if (m_window->ShouldClose())
		{
			break;
		}

		m_window->BeginFrame();
	
		engn::EngineFramesManager::ExecuteFrame();

		if (imGuiIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
		    // Disable warnings, so that they are not spamming when ImGui backend allocates memory not from pools
		    RENDERER_DISABLE_VALIDATION_WARNINGS_SCOPE
		 	ImGui::UpdatePlatformWindows();
		    ImGui::RenderPlatformWindowsDefault();
		}
	}
}

void SculptorEdApplication::OnShutdown()
{
	engn::EngineFramesManager::Shutdown();

	m_renderer.reset();

	rsc::RenderingData::Get().clear();
	
	rdr::UIBackend::Uninitialize();

	rdr::Renderer::WaitIdle();
	
	m_window->UninitializeUI();

	m_window.reset();

	rdr::Renderer::Uninitialize();

	js::JobSystem::Shutdown();

	Super::OnShutdown();
}

void SculptorEdApplication::RenderFrame(SandboxRenderer& renderer)
{
	SPT_PROFILER_FUNCTION();

#if WITH_NSIGHT_CRASH_FIX
	rdr::Renderer::WaitIdle();
#endif // WITH_NSIGHT_CRASH_FIX

	const auto onSwapchainOutOfDateBeforeRendering = [this]()
	{
		ImGui::EndFrame();
		rdr::Renderer::WaitIdle();
		isSwapchainValid = m_window->RebuildSwapchain();
		rdr::Renderer::IncrementFrameReleaseSemaphore(engn::EngineFramesManager::GetRenderingFrame().GetFrameIdx());
	};

	// Additional check in case of changing swapchain settings in application
	if (m_window->IsSwapchainOutOfDate())
	{
		onSwapchainOutOfDateBeforeRendering();
		return;
	}

	if (!isSwapchainValid)
	{
		return;
	}

	const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
	const lib::SharedRef<rdr::Semaphore> acquireSemaphore = rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("AcquireSemaphore"), semaphoreDef);

	const lib::SharedPtr<rdr::Texture> swapchainTexture = m_window->AcquireNextSwapchainTexture(acquireSemaphore);

	if (m_window->IsSwapchainOutOfDate())
	{
		onSwapchainOutOfDateBeforeRendering();
		return;
	}

	js::JobWithResult rendererWaitSemaphore = js::Launch(SPT_GENERIC_JOB_NAME, [&renderer]
														 {
															 const auto res = renderer.RenderFrame();
															 return res;
														 });

	const js::JobWithResult createRenderingContextJob = js::Launch(SPT_GENERIC_JOB_NAME, []
																   {
																	   const auto res = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("MainThreadContext"), rhi::ContextDefinition());
																		return res;
																   });
		

	js::JobWithResult finishSemaphoreJob = js::Launch(SPT_GENERIC_JOB_NAME, []() -> lib::SharedRef<rdr::Semaphore>
													  {
														  const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
														  const auto res = rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);
														  return res;
													  });

	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

	{
		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		const lib::SharedRef<rdr::TextureView> swapchainTextureView = swapchainTexture->CreateView(RENDERER_RESOURCE_NAME("TextureRenderView"), viewDefinition);

		{
			rhi::RHIDependency dependency;
			const SizeType barrierIdx = dependency.AddTextureDependency(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(barrierIdx, rhi::TextureTransition::ColorRenderTarget);

			recorder->ExecuteBarrier(std::move(dependency));
		}

		{
#if WITH_GPU_CRASH_DUMPS
			recorder->SetDebugCheckpoint("Pre UI Render");
#endif // WITH_GPU_CRASH_DUMPS
		}

		{
			rdr::RenderingDefinition renderingDef( math::Vector2i(0, 0), m_window->GetSwapchainSize());
			rdr::RTDefinition renderTarget;
			renderTarget.textureView = swapchainTextureView;
			renderTarget.loadOperation = rhi::ERTLoadOperation::Clear;
			renderTarget.storeOperation = rhi::ERTStoreOperation::Store;
			renderTarget.clearColor.asFloat[0] = 0.f;
			renderTarget.clearColor.asFloat[1] = 0.f;
			renderTarget.clearColor.asFloat[2] = 0.f;
			renderTarget.clearColor.asFloat[3] = 1.f;

			renderingDef.AddColorRenderTarget(renderTarget);

			SPT_GPU_PROFILER_EVENT("UI");
			SPT_GPU_DEBUG_REGION(*recorder, "UI", lib::Color::Green);

			recorder->BeginRendering(renderingDef);

			recorder->RenderUI();

			recorder->EndRendering();
		}

		{
			rhi::RHIDependency dependency;
			const SizeType RTBarrierIdx = dependency.AddTextureDependency(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(RTBarrierIdx, rhi::TextureTransition::PresentSource);

			recorder->ExecuteBarrier(std::move(dependency));
		}

		{
#if WITH_GPU_CRASH_DUMPS
			recorder->SetDebugCheckpoint("Post UI Render");
#endif // WITH_GPU_CRASH_DUMPS
		}
	}

	ImGui::Render();

	rdr::CommandsRecordingInfo recordingInfo;
	recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("TransferCmdBuffer");
	recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	recorder->RecordCommands(createRenderingContextJob.Await(), recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	lib::SharedRef<rdr::Semaphore> finishSemaphore = finishSemaphoreJob.Await();

	lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
	rdr::CommandsSubmitBatch& submitUIBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
	submitUIBatch.recordedCommands.emplace_back(std::move(recorder));
	submitUIBatch.waitSemaphores.AddBinarySemaphore(rendererWaitSemaphore.Await(), rhi::EPipelineStage::ALL_COMMANDS);
	submitUIBatch.signalSemaphores.AddBinarySemaphore(finishSemaphore, rhi::EPipelineStage::ALL_COMMANDS);
	submitUIBatch.signalSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), engn::GetRenderingFrame().GetFrameIdx(), rhi::EPipelineStage::ALL_COMMANDS);

	rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

	rdr::Renderer::PresentTexture(lib::ToSharedRef(m_window), { finishSemaphore });

	if (m_window->IsSwapchainOutOfDate())
	{
		rdr::Renderer::WaitIdle();
		isSwapchainValid = m_window->RebuildSwapchain();
	}
}

void SculptorEdApplication::ExecuteSimulationFrame(engn::FrameContext& context)
{

}

void SculptorEdApplication::ExecuteRenderingFrame(engn::FrameContext& context)
{
	SPT_PROFILER_FUNCTION();

	context.AddFinalizeGPUDelegate(engn::OnFinalizeGPU::Delegate::CreateLambda([](engn::FrameContext& context)
																			   {
																				   rdr::Renderer::WaitForFrameRendered(context.GetFrameIdx());
																			   }));

	const Real32 deltaTime = context.GetDeltaTime();

	rdr::Renderer::BeginFrame();

	rg::RenderGraphManager::OnBeginFrame();

	rdr::UIBackend::BeginFrame();

	ImGui::NewFrame();

	scui::ApplicationUI::Draw(uiContext);

	m_renderer->Tick(deltaTime);

	RenderFrame(*m_renderer);
}

} // spt::ed
