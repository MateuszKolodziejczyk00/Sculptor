#include "SculptorEdApplication.h"
#include "ProfilerCore.h"
#include "GPUApi.h"
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
#include "Pipelines/PSOsLibrary.h"


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
	engn::Engine::Initialize(engineInitializationParams);

	as::AssetsSystemInitializer asInitializer;
	asInitializer.contentPath = engn::GetEngine().GetPaths().contentPath;
	asInitializer.ddcPath     = engn::GetEngine().GetPaths().enginePath / "DDC";

	if (engn::Engine::Get().GetCmdLineArgs().Contains("-CompiledAssetsOnly"))
	{
		lib::AddFlags(asInitializer.flags, as::EAssetsSystemFlags::CompiledOnly);
	}

	engn::Engine::Get().GetAssetsSystem().Initialize(asInitializer);

	const SizeType threadsNum = static_cast<SizeType>(std::thread::hardware_concurrency());

	js::JobSystemInitializationParams jobSystemParams;
	jobSystemParams.workerThreadsNum = threadsNum - 1;
	js::JobSystem::Initialize(jobSystemParams);

	prf::Profiler::Get().Initialize();

	rdr::GPUApi::Initialize();

	rhi::RHIWindowInitializationInfo windowInitInfo;
	windowInitInfo.framebufferSize = math::Vector2u(1920, 1060);
	windowInitInfo.enableVSync     = false;
	m_window = rdr::ResourcesManager::CreateRenderWindow("SculptorEd", windowInitInfo);

	engn::EngineGlobals engineGlobals;
	engineGlobals.profiler           = prf::ProfilerCore::GetInstance().GetProfiler();
	engineGlobals.engineInstance     = &engn::Engine::Get();
	engineGlobals.gpuApiData         = rdr::GPUApi::GetGPUApiData();
	engineGlobals.hashedStringDBData = lib::HashedStringDB::GetDBData();
	engn::Engine::Get().GetModulesManager().InitialzeGlobals(engineGlobals);
}

void SculptorEdApplication::OnRun()
{
	SPT_PROFILER_THREAD("Main Thread");

	Super::OnRun();

	js::Job psoPrecachingJob = js::Launch(SPT_GENERIC_JOB_NAME,
										  []
										  {
											  const rdr::PSOPrecacheParams precacheParams{};
											  rdr::PSOsLibrary::GetInstance().PrecachePSOs(precacheParams);
										  });

	IMGUI_CHECKVERSION();
	uiContext = ImGui::CreateContext();

	const Bool enableViewports = false;

	ImGuiIO& imGuiIO = ImGui::GetIO();
	imGuiIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	if (enableViewports)
	{
		imGuiIO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	}

	imGuiIO.IniFilename = engn::GetEngine().GetPaths().imguiConfigPath.c_str();

	const ImFont* font = imGuiIO.Fonts->AddFontFromFileTTF((engn::GetEngine().GetPaths().contentPath / "Fonts/Inter-Regular.ttf").string().c_str(), 15.f);
	SPT_CHECK(!!font);
	ui::UIUtils::InitializeStyle();

	m_window->InitializeUI(uiContext);
	m_window->GetPlatformWindow().EnableCustomTitleBar();

	ui::UIContextManager::SetGlobalContext(uiContext);

	rdr::UIBackend::Initialize(uiContext, lib::Ref(m_window));

	ImGui::SetCurrentContext(uiContext.GetHandle());

	{
		lib::MemoryArena tempArena("Temp Arena", 1024u * 1024u, 512u * 1024u * 1024u);
		const lib::SharedRef<rdr::RenderContext> renderingContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("InitUIContext"), rhi::ContextDefinition(tempArena));

		const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("InitializeUICommandBuffer"),
																									 renderingContext,
																									 cmdBufferDef);

		recorder->InitializeUIFonts();

		const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

		rdr::GPUApi::GetDeviceQueuesManager().Submit(workload);
		
		workload->Wait();

		rdr::UIBackend::DestroyFontsTemporaryObjects();
	}

	psoPrecachingJob.Wait();

	engn::ModulesManager& modulesManager = engn::Engine::Get().GetModulesManager();

	scui::ViewDefinition sandboxViewDef;
	sandboxViewDef.name = "SandboxView";
	sandboxViewDef.minimumSize = m_window->GetSwapchainSize();
	lib::SharedRef<SandboxUIView> view = scui::ApplicationUI::OpenView<SandboxUIView>(sandboxViewDef);

	lib::SharedPtr<EditorFrameContext> currentFrame;

	js::Event readyForPresentEvent;

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

		if (readyForPresentEvent.IsValid())
		{
			// At this point, window is updated, so we can safely present previous frame
			readyForPresentEvent.Signal();
		}

		if (modulesManager.HasPendingReloads())
		{
			currentFrame->FlushPreviousFrames();
			modulesManager.ProcessModuleReloads();
		}

		readyForPresentEvent = ExecuteFrame(*currentFrame);
	}

	if (readyForPresentEvent.IsValid())
	{
		readyForPresentEvent.Signal();
	}

	if (currentFrame)
	{
		currentFrame->BeginAdvancingStages();
		currentFrame->WaitFrameFinished();
	}
}

void SculptorEdApplication::OnShutdown()
{
	rdr::GPUApi::WaitIdle();

	scui::ApplicationUI::CloseAllViews();

	engn::Engine::Get().GetAssetsSystem().UnloadPermanentAssets();

	engn::EngineFramesManager::Shutdown();

	ecs::GetRegistry().clear();
	
	rdr::UIBackend::Uninitialize();

	m_window->UninitializeUI();

	m_window.reset();
	
	rdr::GPUApi::Uninitialize();

	js::JobSystem::Shutdown();

	prf::ProfilerCore::GetInstance().Shutdown();

	Super::OnShutdown();
}

js::Event SculptorEdApplication::ExecuteFrame(EditorFrameContext& frame)
{
	SPT_PROFILER_FUNCTION();

	as::AssetsSystem& assetsSystem = engn::Engine::Get().GetAssetsSystem();
	if (assetsSystem.HasAssetsToReload())
	{
		frame.FlushPreviousFrames();
		assetsSystem.FlushReloadQueue();
	}

	ImGuiIO& imGuiIO = ImGui::GetIO();

	// Wrap this function in UI stage
	const js::Event uiFinishedEvent = js::CreateEvent("UI FinishedEvent", frame.GetStageFinishedEvent(engn::EFrameStage::UI));

	//const Bool isFocused = m_window->IsFocused();
	//const Real32 targetFPS = isFocused ? -1.f : 5.f;
	//frame.SetMaxFPS(targetFPS);

	frame.BeginAdvancingStages();

	UpdateUI(frame);

	const rhi::EFragmentFormat swapchainFormat = m_window->GetRHI().GetFragmentFormat();

	js::JobWithResult recordUICommandsJob = js::Launch(SPT_GENERIC_JOB_NAME,
													   [this, swapchainFormat, &frame]
													   {
														   return RecordUICommands(frame, swapchainFormat);
													   },
													   js::JobDef().ExecuteBefore(frame.GetStageFinishedEvent(engn::EFrameStage::UpdatingEnd)));

	if (imGuiIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		recordUICommandsJob.Wait();

		// This has to be called from main thread
		ImGui::UpdatePlatformWindows([](void* userData)
									 {
										 EditorFrameContext* frame = static_cast<EditorFrameContext*>(userData);
										 frame->FlushPreviousFrames();
										 rdr::GPUApi::WaitIdle(true);
									 },
									 &frame);
	}

	js::Event readyForPresentEvent = js::CreateEvent("ReadyForPresentEvent", frame.GetStageFinishedEvent(engn::EFrameStage::RenderWindow));

	js::Job renderWindowJob = js::Launch(SPT_GENERIC_JOB_NAME,
										 [this, &frame, recordUICommandsJob]
										 {
											 const rdr::SwapchainTextureHandle swapchainTexture = AcquireSwapchainTexture(frame);
											 const auto [uiRenderContext, uiCommandsWorkload] = recordUICommandsJob.GetResult();
											 RenderFrame(frame, swapchainTexture, uiCommandsWorkload);
										 },
										 js::Prerequisites(frame.GetStageBeginEvent(engn::EFrameStage::RenderWindow),
														   recordUICommandsJob,
														   readyForPresentEvent),
										 js::JobDef()
											 .SetPriority(js::EJobPriority::High)
											 .ExecuteBefore(frame.GetStageFinishedEvent(engn::EFrameStage::RenderWindow)));

	uiFinishedEvent.Signal();

	frame.WaitUpdateEnded();

	return readyForPresentEvent;
}

void SculptorEdApplication::UpdateUI(EditorFrameContext& frame)
{
	SPT_PROFILER_FUNCTION();
	
	rdr::UIBackend::BeginFrame();

	ImGui::NewFrame();

	DrawTitleBar();

	scui::Context uiDrawContext(frame, uiContext, m_window->GetRHI().GetFragmentFormat());
	scui::ApplicationUI::Draw(uiDrawContext);
}

void SculptorEdApplication::DrawTitleBar()
{
	SPT_PROFILER_FUNCTION();

	if (!ImGui::BeginMainMenuBar())
	{
		return;
	}

	platf::PlatformWindow& window = m_window->GetPlatformWindow();

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

	const Real32 height = ImGui::GetFrameHeight();
	const ImVec2 buttonSize(height * 1.5f, height);
	const Real32 iconExtent = height * 0.2f;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 iconColor    = ImGui::GetColorU32(ImGuiCol_Text);
	const ImU32 hoveredColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

	const ImVec2 barPos   = ImGui::GetWindowPos();
	const Real32 barWidth = ImGui::GetWindowWidth();

	drawList->AddLine(ImVec2(barPos.x, barPos.y + height - 0.5f), ImVec2(barPos.x + barWidth, barPos.y + height - 0.5f), ImGui::GetColorU32(ImGuiCol_Border));

	// Centered application name: accent diamond followed by letter-spaced name
	{
		const Real32 tracking       = 2.5f;
		const Real32 diamondExtent  = 3.5f;
		const Real32 diamondSpacing = 10.f;
		const Real32 wordSpacing    = 8.f;

		// Returns x after the text
		const auto drawTracked = [&](Real32 x, lib::StringView text, ImU32 color, Bool draw) -> Real32
		{
			const Real32 y = barPos.y + (height - ImGui::GetFontSize()) * 0.5f;
			for (const char& c : text)
			{
				if (draw)
				{
					drawList->AddText(ImVec2(x, y), color, &c, &c + 1);
				}
				x += ImGui::CalcTextSize(&c, &c + 1).x + tracking;
			}
			return x - tracking;
		};

		const lib::StringView appName  = "SCULPTOR";
		const lib::StringView appScope = "EDITOR";

		const Real32 nameWidth = 2.f * diamondExtent + diamondSpacing + drawTracked(0.f, appName, 0u, false) + wordSpacing + drawTracked(0.f, appScope, 0u, false);

		const ImVec2 diamondCenter(barPos.x + (barWidth - nameWidth) * 0.5f + diamondExtent, barPos.y + height * 0.5f);
		drawList->AddQuadFilled(ImVec2(diamondCenter.x, diamondCenter.y - diamondExtent),
								ImVec2(diamondCenter.x + diamondExtent, diamondCenter.y),
								ImVec2(diamondCenter.x, diamondCenter.y + diamondExtent),
								ImVec2(diamondCenter.x - diamondExtent, diamondCenter.y),
								ImGui::GetColorU32(ImGuiCol_CheckMark));

		const Real32 nameEnd = drawTracked(diamondCenter.x + diamondExtent + diamondSpacing, appName, ImGui::GetColorU32(ImGuiCol_Text), true);
		drawTracked(nameEnd + wordSpacing, appScope, ImGui::GetColorU32(ImGuiCol_TextDisabled), true);
	}

	// Free space before buttons is used for dragging the window
	ImGui::InvisibleButton("##DragArea", ImVec2(barWidth - 3.f * buttonSize.x - ImGui::GetCursorPosX(), height));
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		window.ToggleMaximize();
	}
	else if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		window.BeginDragMove();
	}

	const auto titleBarButton = [&](const char* id, ImU32 buttonHoveredColor, auto&& drawIcon) -> Bool
	{
		ImGui::SameLine();
		const Bool clicked = ImGui::InvisibleButton(id, buttonSize);
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		if (ImGui::IsItemHovered())
		{
			drawList->AddRectFilled(min, max, buttonHoveredColor);
		}
		drawIcon(ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f));
		return clicked;
	};

	if (titleBarButton("##Minimize", hoveredColor, [&](ImVec2 c) { drawList->AddLine(ImVec2(c.x - iconExtent, c.y), ImVec2(c.x + iconExtent, c.y), iconColor); }))
	{
		window.Minimize();
	}

	if (titleBarButton("##Maximize", hoveredColor, [&](ImVec2 c) { drawList->AddRect(ImVec2(c.x - iconExtent, c.y - iconExtent), ImVec2(c.x + iconExtent, c.y + iconExtent), iconColor); }))
	{
		window.ToggleMaximize();
	}

	if (titleBarButton("##Close", IM_COL32(196, 43, 28, 255), [&](ImVec2 c)
					   {
						   drawList->AddLine(ImVec2(c.x - iconExtent, c.y - iconExtent), ImVec2(c.x + iconExtent, c.y + iconExtent), iconColor);
						   drawList->AddLine(ImVec2(c.x - iconExtent, c.y + iconExtent), ImVec2(c.x + iconExtent, c.y - iconExtent), iconColor);
					   }))
	{
		window.Close();
	}

	ImGui::PopStyleVar();

	ImGui::EndMainMenuBar();
}

std::pair<lib::SharedRef<rdr::RenderContext>, lib::SharedRef<rdr::GPUWorkload>> SculptorEdApplication::RecordUICommands(EditorFrameContext& frame, rhi::EFragmentFormat rtFormat) const
{
	SPT_PROFILER_FUNCTION();

	ui::UIUtils::DrawDockTabsAccent();
	ImGui::Render();

	lib::MemoryArena tempArena = frame.GetFrameMemoryArena().CreateSubArena("Record UI Commands Temp Arena", 8u * 1024u);
	const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("UI Render Context"), rhi::ContextDefinition(tempArena));

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

	// We want to submit cmd list even if we can't present anything, even if that cmd list is empty
	// this allows signaling fence and it generally simplifies frame management
	const Bool canPresent = swapchainTextureHandle.IsValid() && !m_window->IsSwapchainOutOfDate();

	const lib::SharedPtr<rdr::Texture> swapchainTexture = m_window->GetSwapchainTexture(swapchainTextureHandle);

	lib::MemoryArena renderFrameArena = frame.GetFrameMemoryArena().CreateSubArena("Render Frame Arena", 32u * 1024u);
	const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("MainThreadContext"), rhi::ContextDefinition(renderFrameArena));
		
	const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary);
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("MainThreadCmdBuffer"),
																								 renderContext,
																								 cmdBufferDef);

	if (canPresent)
	{
		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		const lib::SharedRef<rdr::TextureView> swapchainTextureView = swapchainTexture->CreateView(RENDERER_RESOURCE_NAME("TextureRenderView"), viewDefinition);

		{
			rhi::RHIDependency dependency;
			const SizeType barrierIdx = dependency.AddTextureDependency(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(barrierIdx, rhi::TextureTransition::Undefined, rhi::TextureTransition::ColorRenderTarget);

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

	if (canPresent)
	{
		workload->GetWaitSemaphores().AddBinarySemaphore(swapchainTextureHandle.GetAcquireSemaphore(), rhi::EPipelineStage::TopOfPipe);
	}

	const lib::SharedRef<rdr::Semaphore> uiFinishedSemaphore = workload->AddBinarySignalSemaphore(RENDERER_RESOURCE_NAME("UI Finished Semaphore"), rhi::EPipelineStage::ALL_COMMANDS);

	rdr::GPUApi::GetDeviceQueuesManager().Submit(workload, rdr::EGPUWorkloadSubmitFlags::CorePipe);

	frame.SetFrameFinishedOnGPUWaitable(lib::MakeShared<rdr::GPUWaitableEvent>(workload));

	if (canPresent)
	{
		rdr::GPUApi::GetDeviceQueuesManager().Present(lib::ToSharedRef(m_window), swapchainTextureHandle, { uiFinishedSemaphore });
	}
}

} // spt::ed
