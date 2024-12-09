#include "SculptorEdApplication.h"
#include "ProfilerCore.h"
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
#include "Paths.h"
#include "ECSRegistry.h"
#include "DeviceQueues/GPUWorkload.h"
#include "Utils/GPUWaitableEvent.h"
#include "EditorFrame.h"
#include "GlobalResources/GlobalResourcesRegistry.h"


namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(AppLog, true)


SculptorEdApplication::SculptorEdApplication()
{ }

void SculptorEdApplication::OnInit(int argc, char** argv)
{
	Super::OnInit(argc, argv);

	prf::ProfilerCore::GetInstance().Initialize();

	engn::EngineInitializationParams engineInitializationParams;
	engineInitializationParams.cmdLineArgsNum = argc;
	engineInitializationParams.cmdLineArgs    = argv;
	engn::Engine::Get().Initialize(engineInitializationParams);

	const SizeType threadsNum = static_cast<SizeType>(std::thread::hardware_concurrency());

	js::JobSystemInitializationParams jobSystemParams;
	jobSystemParams.workerThreadsNum = threadsNum - 1;
	js::JobSystem::Initialize(jobSystemParams);

	prf::Profiler::Get().Initialize();

	rdr::Renderer::Initialize();

	rhi::RHIWindowInitializationInfo windowInitInfo;
	windowInitInfo.framebufferSize = math::Vector2u(1920, 1080);
	windowInitInfo.enableVSync     = false;
	m_window = rdr::ResourcesManager::CreateWindow("SculptorEd", windowInitInfo);

	gfx::global::GlobalResourcesRegistry::Get().InitializeAll();
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

	ImGui::SetCurrentContext(uiContext.GetHandle());

	{
		const lib::SharedRef<rdr::RenderContext> renderingContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("InitUIContext"), rhi::ContextDefinition());

		const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("InitializeUICommandBuffer"),
																									 renderingContext,
																									 cmdBufferDef);

		recorder->InitializeUIFonts();

		const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

		rdr::Renderer::GetDeviceQueuesManager().Submit(workload);
		
		workload->Wait();

		rdr::UIBackend::DestroyFontsTemporaryObjects();
	}

	scui::ViewDefinition sandboxViewDef;
	sandboxViewDef.name = "SandboxView";
	sandboxViewDef.minimumSize = m_window->GetSwapchainSize();
	lib::SharedRef<SandboxUIView> view = scui::ApplicationUI::OpenView<SandboxUIView>(sandboxViewDef);

	lib::SharedPtr<EditorFrameContext> currentFrame;

	while (true)
	{
		SPT_PROFILER_FRAME("EditorFrame");

		currentFrame = engn::EngineFramesManager::CreateFrame<EditorFrameContext>();

		m_window->Update();

		if (m_window->ShouldClose())
		{
			break;
		}

		m_window->BeginFrame();

		ExecuteFrame(*currentFrame);
	}

	if (currentFrame)
	{
		currentFrame->BeginAdvancingStages();
		currentFrame->WaitFrameFinished();
	}
}

void SculptorEdApplication::OnShutdown()
{
	rdr::Renderer::WaitIdle();

	scui::ApplicationUI::CloseAllViews();

	engn::EngineFramesManager::Shutdown();

	ecs::GetRegistry().clear();
	
	rdr::UIBackend::Uninitialize();

	m_window->UninitializeUI();

	m_window.reset();
	
	rdr::Renderer::Uninitialize();

	js::JobSystem::Shutdown();

	prf::ProfilerCore::GetInstance().Shutdown();

	Super::OnShutdown();
}

void SculptorEdApplication::ExecuteFrame(EditorFrameContext& frame)
{
	SPT_PROFILER_FUNCTION();

	const rdr::SwapchainTextureHandle swapchainTexture = AcquireSwapchainTexture(frame);
	if (!swapchainTexture.IsValid())
	{
		// skip frame
		frame.BeginAdvancingStages();
		return;
	}

	const js::Event uiFinishedEvent = js::CreateEvent("UI FinishedEvent", frame.GetStageFinishedEvent(engn::EFrameStage::UI));

	const Bool isFocused = m_window->IsFocused();
	const Real32 targetFPS = isFocused ? -1.f : 5.f;
	frame.SetMaxFPS(targetFPS);

	frame.BeginAdvancingStages();

	UpdateUI(frame);

	const rhi::EFragmentFormat swapchainFormat = m_window->GetRHI().GetFragmentFormat();

	js::JobWithResult recordUICommandsJob = js::Launch(SPT_GENERIC_JOB_NAME,
													   [this, swapchainFormat]
													   {
														   return RecordUICommands(swapchainFormat);
													   },
													   js::JobDef().ExecuteBefore(frame.GetStageFinishedEvent(engn::EFrameStage::UpdatingEnd)));

	js::Job renderWindowJob = js::Launch(SPT_GENERIC_JOB_NAME,
										 [this, &frame, swapchainTexture, recordUICommandsJob]
										 {
											 const auto [uiRenderContext, uiCommandsWorkload] = recordUICommandsJob.GetResult();
											 RenderFrame(frame, swapchainTexture, uiCommandsWorkload);
										 },
										 js::Prerequisites(frame.GetStageBeginEvent(engn::EFrameStage::RenderWindow),
														   recordUICommandsJob),
										 js::JobDef()
											 .SetPriority(js::EJobPriority::High)
											 .ExecuteBefore(frame.GetStageFinishedEvent(engn::EFrameStage::RenderWindow)));

	ImGuiIO& imGuiIO = ImGui::GetIO();
	if (imGuiIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		recordUICommandsJob.Wait();

		// Flush previous frames if we need to close any window
		const ImGuiContext* context = ImGui::GetCurrentContext();
		for (int i = 1; i < context->Viewports.Size; i++)
		{
			ImGuiViewportP* viewport = context->Viewports[i];
			ImGuiWindow* window = viewport->Window;
			if (window && (!window->Active || window->Hidden))
			{
				frame.FlushPreviousFrames();
				break;
			}
		}

		// This has to be called from main thread
		ImGui::UpdatePlatformWindows();
	}

	uiFinishedEvent.Signal();

	frame.WaitUpdateEnded();
}

void SculptorEdApplication::UpdateUI(EditorFrameContext& frame)
{
	SPT_PROFILER_FUNCTION();
	
	rdr::UIBackend::BeginFrame();

	ImGui::NewFrame();

	scui::Context uiDrawContext(frame, uiContext, m_window->GetRHI().GetFragmentFormat());
	scui::ApplicationUI::Draw(uiDrawContext);
}

std::pair<lib::SharedRef<rdr::RenderContext>, lib::SharedRef<rdr::GPUWorkload>> SculptorEdApplication::RecordUICommands(rhi::EFragmentFormat rtFormat) const
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("UI Render Context"), rhi::ContextDefinition());

	ImGui::Render();

	rhi::RenderingInheritanceDefinition renderingInheritanceDef;
	renderingInheritanceDef.colorRTFormats.emplace_back(rtFormat);
	
	rhi::CommandBufferUsageDefinition cmdBufferUsageDef;
	cmdBufferUsageDef.beginFlags           = lib::Flags(rhi::ECommandBufferBeginFlags::OneTimeSubmit, rhi::ECommandBufferBeginFlags::ContinueRendering);
	cmdBufferUsageDef.renderingInheritance = std::move(renderingInheritanceDef);

	const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Secondary, rhi::ECommandBufferComplexityClass::Low);
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("Render UI Secondary Cmd Buffer"),
																								 renderContext,
																								 cmdBufferDef,
																								 cmdBufferUsageDef);

	recorder->RenderUI();
		
	const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

	return std::make_pair(renderContext, workload);
}

rdr::SwapchainTextureHandle SculptorEdApplication::AcquireSwapchainTexture(EditorFrameContext& frame)
{
	SPT_PROFILER_FUNCTION();

	if (m_window->IsSwapchainOutOfDate())
	{
		frame.FlushPreviousFrames();
		m_window->RebuildSwapchain();
	}

	// swapchain still may be invalid after rebuild if window is minimized
	if (!m_window->IsSwapchainValid())
	{
		return rdr::SwapchainTextureHandle{};
	}

	const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
	const lib::SharedRef<rdr::Semaphore> acquireSemaphore = rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("AcquireSemaphore"), semaphoreDef);

	const rdr::SwapchainTextureHandle swapchainTexture = m_window->AcquireNextSwapchainTexture(acquireSemaphore);

	return swapchainTexture;
}

void SculptorEdApplication::RenderFrame(EditorFrameContext& frame, rdr::SwapchainTextureHandle swapchainTextureHandle, const lib::SharedRef<rdr::GPUWorkload>& recordedUIWorkload)
{
	SPT_PROFILER_FUNCTION();

#if WITH_NSIGHT_CRASH_FIX
	rdr::Renderer::WaitIdle();
#endif // WITH_NSIGHT_CRASH_FIX

	if (!swapchainTextureHandle.IsValid() || m_window->IsSwapchainOutOfDate())
	{
		return;
	}

	const lib::SharedPtr<rdr::Texture> swapchainTexture = m_window->GetSwapchainTexture(swapchainTextureHandle);

	const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("MainThreadContext"), rhi::ContextDefinition());
		
	const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary);
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("MainThreadCmdBuffer"),
																								 renderContext,
																								 cmdBufferDef);

	{
		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		const lib::SharedRef<rdr::TextureView> swapchainTextureView = swapchainTexture->CreateView(RENDERER_RESOURCE_NAME("TextureRenderView"), viewDefinition);

		{
			rhi::RHIDependency dependency;
			const SizeType barrierIdx = dependency.AddTextureDependency(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(barrierIdx, rhi::TextureTransition::ColorRenderTarget);

			recorder->ExecuteBarrier(dependency);
		}

		{
#if WITH_GPU_CRASH_DUMPS
			recorder->SetDebugCheckpoint("Pre UI Render");
#endif // WITH_GPU_CRASH_DUMPS
		}

		{
			rdr::RenderingDefinition renderingDef( math::Vector2i(0, 0), m_window->GetSwapchainSize(), lib::Flags(rhi::ERenderingFlags::Default, rhi::ERenderingFlags::ContentsSecondaryCmdBuffers));
			rdr::RTDefinition renderTarget;
			renderTarget.textureView = swapchainTextureView;
			renderTarget.loadOperation = rhi::ERTLoadOperation::Clear;
			renderTarget.storeOperation = rhi::ERTStoreOperation::Store;
			renderTarget.clearColor.asFloat[0] = 0.f;
			renderTarget.clearColor.asFloat[1] = 0.f;
			renderTarget.clearColor.asFloat[2] = 0.f;
			renderTarget.clearColor.asFloat[3] = 1.f;

			renderingDef.AddColorRenderTarget(renderTarget);

			SPT_GPU_DEBUG_REGION(*recorder, "UI", lib::Color::Green);

			recorder->BeginRendering(renderingDef);

			recorder->ExecuteCommands(recordedUIWorkload);

			recorder->EndRendering();
		}

		{
			rhi::RHIDependency dependency;

			const SizeType RTBarrierIdx = dependency.AddTextureDependency(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(RTBarrierIdx, rhi::TextureTransition::PresentSource);

			recorder->ExecuteBarrier(dependency);
		}

		{
#if WITH_GPU_CRASH_DUMPS
			recorder->SetDebugCheckpoint("Post UI Render");
#endif // WITH_GPU_CRASH_DUMPS
		}
	}

	const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

	workload->GetWaitSemaphores().AddBinarySemaphore(swapchainTextureHandle.GetAcquireSemaphore(), rhi::EPipelineStage::TopOfPipe);

	const lib::SharedRef<rdr::Semaphore> uiFinishedSemaphore = workload->AddBinarySignalSemaphore(RENDERER_RESOURCE_NAME("UI Finished Semaphore"), rhi::EPipelineStage::ALL_COMMANDS);

	rdr::Renderer::GetDeviceQueuesManager().Submit(workload, rdr::EGPUWorkloadSubmitFlags::CorePipe);

	frame.SetFrameFinishedOnGPUWaitable(lib::MakeShared<rdr::GPUWaitableEvent>(workload));

	rdr::Renderer::PresentTexture(lib::ToSharedRef(m_window), swapchainTextureHandle, { uiFinishedSemaphore });
}

} // spt::ed
