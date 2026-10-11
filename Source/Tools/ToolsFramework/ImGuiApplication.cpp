#include "ImGuiApplication.h"
#include "Engine.h"
#include "GPUApi.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "Types/Window.h"
#include "Types/Semaphore.h"
#include "Types/Texture.h"
#include "Types/RenderContext.h"
#include "Types/UIBackend.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "CommandsRecorder/RenderingDefinition.h"
#include "DeviceQueues/GPUWorkload.h"
#include "Utils/TransfersUtils.h"
#include "UIContextManager.h"
#include "ImGui/SculptorImGui.h"
#include "EngineFrame.h"
#include "UIElements/ApplicationUI.h"
#include "RenderGraphCapturer.h"
#include "RenderGraphCaptureSourceContext.h"
#include "RenderGraphCaptureViewer.h"


namespace spt::tools
{

ImGuiApplication::ImGuiApplication(lib::StringView name, math::Vector2u windowResolution /*= math::Vector2u(1600u, 900u)*/)
	: m_frameArena("ImGui Application Frame Arena", 1024u * 1024u, 512u * 1024u * 1024u)
	, m_captureSourceContext(lib::MakeShared<rg::capture::RGCaptureSourceContext>())
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIWindowInitializationInfo windowInitInfo;
	windowInitInfo.framebufferSize = windowResolution;
	windowInitInfo.enableVSync     = true;
	m_window = rdr::ResourcesManager::CreateRenderWindow(name, windowInitInfo);

	const lib::Path imGuiConfigPath = engn::GetEngine().GetPaths().savedPath / "Tools" / (lib::String(name) + "_ImGui.ini");
	std::filesystem::create_directories(imGuiConfigPath.parent_path());
	m_imGuiConfigPath = imGuiConfigPath.generic_string();

	IMGUI_CHECKVERSION();
	m_uiContext = ImGui::CreateContext();

	ImGuiIO& imGuiIO = ImGui::GetIO();
	imGuiIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	imGuiIO.IniFilename = m_imGuiConfigPath.c_str();

	m_window->InitializeUI(m_uiContext);

	ui::UIContextManager::SetGlobalContext(m_uiContext);

	rdr::UIBackend::Initialize(m_uiContext, lib::Ref(m_window));

	InitializeUIFonts();
}

ImGuiApplication::~ImGuiApplication()
{
	SPT_PROFILER_FUNCTION();

	rdr::GPUApi::WaitIdle();

	m_pendingCapture.reset();

	scui::ApplicationUI::CloseAllViews();

	engn::EngineFramesManager::Shutdown();

	m_lastFrameWorkload.reset();

	rdr::UIBackend::Uninitialize();

	m_window->UninitializeUI();

	m_window.reset();
}

void ImGuiApplication::Run()
{
	while (true)
	{
		m_window->Update();

		if (m_window->ShouldClose())
		{
			break;
		}

		ExecuteFrame();
	}

	rdr::GPUApi::WaitIdle();
}

void ImGuiApplication::RequestRenderGraphCapture()
{
	m_wantsRenderGraphCapture = true;
}

void ImGuiApplication::InitializeUIFonts()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("Initialize UI Fonts Context"), rhi::ContextDefinition(m_frameArena));

	const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("Initialize UI Fonts Cmd Buffer"),
																								 renderContext,
																								 cmdBufferDef);

	recorder->InitializeUIFonts();

	const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

	rdr::GPUApi::GetDeviceQueuesManager().Submit(workload);

	workload->Wait();

	rdr::UIBackend::DestroyFontsTemporaryObjects();
}

void ImGuiApplication::ExecuteFrame()
{
	SPT_PROFILER_FUNCTION();

	// Only one frame can be in flight, so all resources used by previous frame can be safely reused
	if (m_lastFrameWorkload)
	{
		m_lastFrameWorkload->Wait();
		m_lastFrameWorkload.reset();
	}

	rdr::GPUApi::GetDeviceQueuesManager().AdvanceGPUTimelineSection();
	rdr::GPUApi::FlushCaches();
	rdr::GPUApi::ScheduleFlushDeferredReleases();
	rdr::GPUApi::FlushReadyReleases();

	if (m_pendingCapture)
	{
		OpenCaptureViewer(std::move(m_pendingCapture));
	}

	m_frameArena.Reset();

	const lib::SharedPtr<engn::FrameContext> frame = engn::EngineFramesManager::CreateFrame<engn::FrameContext>();

	m_window->BeginFrame();
	rdr::UIBackend::BeginFrame();
	ImGui::NewFrame();

	const scui::Context uiDrawContext(*frame, m_uiContext, m_window->GetRHI().GetFragmentFormat());
	scui::ApplicationUI::Draw(uiDrawContext);

	DrawUI();

	ImGui::Render();

	frame->BeginAdvancingStages();
	frame->WaitFrameFinished();

	ExecuteRenderGraph();

	RenderUI();
}

void ImGuiApplication::ExecuteRenderGraph()
{
	SPT_PROFILER_FUNCTION();

	rg::RenderGraphBuilder graphBuilder(m_frameArena, m_renderGraphResourcesPool);

	rg::capture::RGCaptureSourceInfo captureSource;
	captureSource.sourceContext = m_captureSourceContext;

	rg::capture::RenderGraphCapturer capturer(rg::capture::CaptureParameters{}, captureSource);

	if (m_wantsRenderGraphCapture)
	{
		capturer.Capture(graphBuilder);
		m_wantsRenderGraphCapture = false;
	}

	m_captureSourceContext->ExecuteOnSetupNewGraphBuilder(graphBuilder);

	RenderFrame(graphBuilder);

	rdr::FlushPendingUploads();

	graphBuilder.Execute();

	if (capturer.HasValidCapture())
	{
		m_pendingCapture = capturer.GetCapture();
	}
}

void ImGuiApplication::OpenCaptureViewer(lib::SharedPtr<rg::capture::RGCapture> capture)
{
	SPT_PROFILER_FUNCTION();

	scui::ViewDefinition viewDef("Render Graph Capture Viewer");
	viewDef.minimumSize = math::Vector2u(800u, 600u);
	scui::ApplicationUI::OpenView<rg::capture::RenderGraphCaptureViewer>(viewDef, lib::Ref(capture));
}

void ImGuiApplication::RenderUI()
{
	SPT_PROFILER_FUNCTION();

	if (m_window->IsSwapchainOutOfDate())
	{
		rdr::GPUApi::WaitIdle();
		m_window->RebuildSwapchain();
	}

	// Swapchain may be invalid even after rebuild if window is minimized
	rdr::SwapchainTextureHandle swapchainTextureHandle;
	if (m_window->IsSwapchainValid())
	{
		const lib::SharedRef<rdr::Semaphore> acquireSemaphore = rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("Acquire Semaphore"), rhi::SemaphoreDefinition(rhi::ESemaphoreType::Binary));
		swapchainTextureHandle = m_window->AcquireNextSwapchainTexture(acquireSemaphore);
	}

	const Bool canPresent = swapchainTextureHandle.IsValid() && !m_window->IsSwapchainOutOfDate();

	const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("UI Render Context"), rhi::ContextDefinition(m_frameArena));

	const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary);
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("UI Cmd Buffer"),
																								 renderContext,
																								 cmdBufferDef);

	if (canPresent)
	{
		const lib::SharedPtr<rdr::Texture> swapchainTexture = m_window->GetSwapchainTexture(swapchainTextureHandle);

		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		const lib::SharedRef<rdr::TextureView> swapchainTextureView = swapchainTexture->CreateView(RENDERER_RESOURCE_NAME("Swapchain Texture View"), viewDefinition);

		{
			rhi::RHIDependency dependency;
			const SizeType barrierIdx = dependency.AddTextureDependency(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(barrierIdx, rhi::TextureTransition::Undefined, rhi::TextureTransition::ColorRenderTarget);
			recorder->ExecuteBarrier(dependency);
		}

		{
			rdr::RenderingDefinition renderingDef(math::Vector2i(0, 0), m_window->GetSwapchainSize());

			rdr::RTDefinition renderTarget;
			renderTarget.textureView    = swapchainTextureView;
			renderTarget.loadOperation  = rhi::ERTLoadOperation::Clear;
			renderTarget.storeOperation = rhi::ERTStoreOperation::Store;
			renderTarget.clearColor.asFloat[0] = 0.f;
			renderTarget.clearColor.asFloat[1] = 0.f;
			renderTarget.clearColor.asFloat[2] = 0.f;
			renderTarget.clearColor.asFloat[3] = 1.f;
			renderingDef.AddColorRenderTarget(renderTarget);

			recorder->BeginRendering(renderingDef);
			recorder->RenderUI();
			recorder->EndRendering();
		}

		{
			rhi::RHIDependency dependency;
			const SizeType barrierIdx = dependency.AddTextureDependency(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(barrierIdx, rhi::TextureTransition::PresentSource);
			recorder->ExecuteBarrier(dependency);
		}
	}

	const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

	lib::SharedPtr<rdr::Semaphore> uiFinishedSemaphore;
	if (canPresent)
	{
		workload->GetWaitSemaphores().AddBinarySemaphore(swapchainTextureHandle.GetAcquireSemaphore(), rhi::EPipelineStage::TopOfPipe);
		uiFinishedSemaphore = workload->AddBinarySignalSemaphore(RENDERER_RESOURCE_NAME("UI Finished Semaphore"), rhi::EPipelineStage::ALL_COMMANDS).ToSharedPtr();
	}

	rdr::GPUApi::GetDeviceQueuesManager().Submit(workload, rdr::EGPUWorkloadSubmitFlags::CorePipe);

	if (canPresent)
	{
		rdr::GPUApi::GetDeviceQueuesManager().Present(lib::Ref(m_window), swapchainTextureHandle, { uiFinishedSemaphore });
	}

	m_lastFrameWorkload = workload.ToSharedPtr();
}

} // spt::tools
