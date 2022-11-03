#include "SculptorEdApplication.h"
#include "Renderer.h"
#include "ResourcesManager.h"
#include "Timer/TickingTimer.h"
#include "Types/Semaphore.h"
#include "Types/Texture.h"
#include "Types/RenderContext.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "CommandsRecorder/RenderingDefinition.h"
#include "UIContextManager.h"
#include "Profiler.h"
#include "GPUDiagnose/Diagnose.h"
#include "Engine.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Common/ShaderCompilationInput.h"
#include "ImGui/SculptorImGui.h"
#include "Types/Sampler.h"
#include "Bindings/StorageTextureBinding.h"
#include "Bindings/ConstantBufferBinding.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Containers/DynamicInlineArray.h"
#include "JobSystem/JobSystem.h"
#include "UIElements/UIWindow.h"
#include "UIElements/ApplicationUI.h"
#include "UI/SandboxUILayer.h"
#include "Renderer/SandboxRenderer.h"
#include "UIUtils.h"
#include "ProfilerUILayer.h"


namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(AppLog, true)


SculptorEdApplication::SculptorEdApplication()
{ }

void SculptorEdApplication::OnInit(int argc, char** argv)
{
	Super::OnInit(argc, argv);

	engn::EngineInitializationParams engineInitializationParams;
	engineInitializationParams.cmdLineArgsNum = argc;
	engineInitializationParams.cmdLineArgs = argv;
	engn::Engine::Initialize(engineInitializationParams);

	const SizeType threadsNum = static_cast<SizeType>(std::thread::hardware_concurrency());

	js::JobSystemInitializationParams jobSystemParams;
	jobSystemParams.workerThreadsNum = threadsNum - 1;
	js::JobSystem::Initialize(jobSystemParams);

	rdr::Renderer::Initialize();

	m_window = rdr::ResourcesManager::CreateWindow("SculptorEd", math::Vector2u(1920, 1080));

	rdr::Renderer::PostCreatedWindow();
}

void SculptorEdApplication::OnRun()
{
	Super::OnRun();

	IMGUI_CHECKVERSION();
	const ui::UIContext context = ImGui::CreateContext();

	ImGuiIO& imGuiIO = ImGui::GetIO();
	imGuiIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	imGuiIO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	m_window->InitializeUI(context);

	ui::UIContextManager::SetGlobalContext(context);

	rdr::UIBackend::Initialize(context, lib::Ref(m_window));

	ImGui::SetCurrentContext(context.GetHandle());

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

	SandboxRenderer renderer(m_window);

	scui::LayerDefinition sandboxLayerDef;
	sandboxLayerDef.name = "SandboxLayer";
	scui::ApplicationUI::OpenWindowWithLayer<SandboxUILayer>("SandboxWindow", sandboxLayerDef, renderer);
	
	scui::LayerDefinition profilerLayerDef;
	profilerLayerDef.name = "ProfilerLayer";
	scui::ApplicationUI::OpenWindowWithLayer<prf::ProfilerUILayer>("ProfilerWindow", profilerLayerDef);

	lib::TickingTimer timer;
	Real32 time = 0.f;

	while (true)
	{
		SPT_PROFILER_FRAME("EditorFrame");

		const Real32 deltaTime = timer.Tick();
		time += deltaTime;

		m_window->Update(deltaTime);

		if (m_window->ShouldClose())
		{
			break;
		}

		rdr::Renderer::BeginFrame();

		m_window->BeginFrame();

		rdr::UIBackend::BeginFrame();

		ImGui::NewFrame();


		renderer.GetDescriptorSet().u_viewInfo.Set([time](TestViewInfo& info)
												   {
													   info.color = math::Vector4f::Constant(sin(time));
												   });

		scui::ApplicationUI::Draw(context);
		ImGui::Render();
		RenderFrame(renderer);

		if (imGuiIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
		    // Disable warnings, so that they are not spamming when ImGui backend allocates memory not from pools
		    RENDERER_DISABLE_VALIDATION_WARNINGS_SCOPE
		 	ImGui::UpdatePlatformWindows();
		    ImGui::RenderPlatformWindowsDefault();
		}
	
		rdr::Renderer::EndFrame();
	}
}

void SculptorEdApplication::OnShutdown()
{
	Super::OnShutdown();
	
	rdr::UIBackend::Uninitialize();

	rdr::Renderer::WaitIdle();
	
	m_window->UninitializeUI();

	m_window.reset();

	rdr::Renderer::Uninitialize();

	js::JobSystem::Shutdown();
}

void SculptorEdApplication::RenderFrame(SandboxRenderer& renderer)
{
	SPT_PROFILER_FUNCTION();

	const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
	const lib::SharedRef<rdr::Semaphore> acquireSemaphore = rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("AcquireSemaphore"), semaphoreDef);

	const lib::SharedPtr<rdr::Texture> swapchainTexture = m_window->AcquireNextSwapchainTexture(acquireSemaphore);

	if (m_window->IsSwapchainOutOfDate())
	{
		rdr::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
		rdr::Renderer::IncrementReleaseSemaphoreToCurrentFrame();
		return;
	}

	js::JobWithResult rendererWaitSemaphore = js::Launch([&renderer]
														 {
															 return renderer.RenderFrame();
														 });

	const js::JobWithResult createRenderingContextJob = js::Launch([]
																   {
																	   return rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("MainThreadContext"), rhi::ContextDefinition());
																   });
		

	js::JobWithResult finishSemaphoreJob = js::Launch([]() -> lib::SharedRef<rdr::Semaphore>
													  {
														  const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
														  return rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);
													  });

	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

	{
		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		const lib::SharedRef<rdr::TextureView> swapchainTextureView = swapchainTexture->CreateView(RENDERER_RESOURCE_NAME("TextureRenderView"), viewDefinition);

		{
			rdr::Barrier barrier = rdr::ResourcesManager::CreateBarrier();
			const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::ColorRenderTarget);

			recorder->ExecuteBarrier(std::move(barrier));
		}

		{
			rdr::RenderingDefinition renderingDef(rhi::ERenderingFlags::None, math::Vector2i(0, 0), m_window->GetSwapchainSize());
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
			rdr::Barrier barrier = rdr::ResourcesManager::CreateBarrier();
			const SizeType RTBarrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(RTBarrierIdx, rhi::TextureTransition::PresentSource);

			recorder->ExecuteBarrier(std::move(barrier));
		}
	}

	rdr::CommandsRecordingInfo recordingInfo;
	recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("TransferCmdBuffer");
	recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	recorder->RecordCommands(createRenderingContextJob.Await(), recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	lib::SharedRef<rdr::Semaphore> finishSemaphore = finishSemaphoreJob.Await();

	lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
	rdr::CommandsSubmitBatch& submitUIBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
	submitUIBatch.recordedCommands.emplace_back(std::move(recorder));
	submitUIBatch.waitSemaphores.AddBinarySemaphore(rendererWaitSemaphore.Await(), rhi::EPipelineStage::TopOfPipe);
	submitUIBatch.signalSemaphores.AddBinarySemaphore(finishSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitUIBatch.signalSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), rdr::Renderer::GetCurrentFrameIdx(), rhi::EPipelineStage::TopOfPipe);

	rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

	rdr::Renderer::PresentTexture(lib::ToSharedRef(m_window), { finishSemaphore });

	if (m_window->IsSwapchainOutOfDate())
	{
		rdr::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
	}
}

} // spt::ed
