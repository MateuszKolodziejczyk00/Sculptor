#include "SculptorEdApplication.h"
#include "Renderer.h"
#include "RendererBuilder.h"
#include "Profiler.h"
#include "Timer/TickingTimer.h"
#include "Types/Semaphore.h"
#include "Types/Texture.h"
#include "CommandsRecorder/CommandsRecorder.h"
#include "CommandsRecorder/RenderingDefinition.h"
#include "UIContextManager.h"
#include "Engine.h"
#include "imgui.h"

#include "Shaders/ShadersManager.h"


namespace spt::ed
{

SculptorEdApplication::SculptorEdApplication()
{
}

void SculptorEdApplication::OnInit(int argc, char** argv)
{
	Super::OnInit(argc, argv);

	engn::EngineInitializationParams engineInitializationParams;
	engineInitializationParams.cmdLineArgsNum = argc;
	engineInitializationParams.cmdLineArgs = argv;
	engn::Engine::Initialize(engineInitializationParams);

	renderer::Renderer::Initialize();

	m_window = renderer::RendererBuilder::CreateWindow("SculptorEd", math::Vector2u(1920, 1080));

	renderer::Renderer::PostCreatedWindow();

	renderer::Renderer::GetShadersManager().GetShader("Test.glsl", sc::ShaderCompilationSettings());
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

	uiBackend = renderer::RendererBuilder::CreateUIBackend(context, m_window);

	ImGui::SetCurrentContext(context.GetHandle());

	{
		renderer::CommandsRecordingInfo recordingInfo;
		recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("InitializeUICommandBuffer");
		recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		lib::UniquePtr<renderer::CommandsRecorder> recorder = renderer::Renderer::StartRecordingCommands(recordingInfo);

		recorder->StartRecording(rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

		recorder->InitializeUIFonts(uiBackend);

		recorder->FinishRecording();
		
		lib::DynamicArray<renderer::CommandsSubmitBatch> submitBatches;
		renderer::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(renderer::CommandsSubmitBatch());
		submitBatch.recordedCommands.emplace_back(std::move(recorder));

		renderer::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

		renderer::Renderer::WaitIdle();

		uiBackend->DestroyFontsTemporaryObjects();
	}

	lib::TickingTimer timer;

	while (true)
	{
		SPT_PROFILE_FRAME("EditorFrame");

		const Real32 deltaTime = timer.Tick();

		m_window->Update(deltaTime);

		if (m_window->ShouldClose())
		{
			break;
		}

		renderer::Renderer::BeginFrame();

		m_window->BeginFrame();

		uiBackend->BeginFrame();

		ImGui::NewFrame();

		ImGui::Begin("Test");
		ImGui::Text("TESTSDasd");
		ImGui::End();
		ImGui::Begin("ASD");
		ImGui::Text("TESTSDasd");
		ImGui::End();

		ImGui::Render();
		
		if (imGuiIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			// Disable warnings, so that they are not spamming when ImGui backend allocates memory not from pools
			renderer::Renderer::EnableValidationWarnings(false);
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			renderer::Renderer::EnableValidationWarnings(true);
		}

		RenderFrame();

		renderer::Renderer::EndFrame();
	}
}

void SculptorEdApplication::OnShutdown()
{
	Super::OnShutdown();

	uiBackend.reset();

	renderer::Renderer::WaitIdle();
	
	m_window->UninitializeUI();

	m_window.reset();

	renderer::Renderer::Uninitialize();
}

void SculptorEdApplication::RenderFrame()
{
	const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
	const lib::SharedPtr<renderer::Semaphore> acquireSemaphore = renderer::RendererBuilder::CreateSemaphore(RENDERER_RESOURCE_NAME("AcquireSemaphore"), semaphoreDef);

	const lib::SharedPtr<renderer::Texture> swapchainTexture = m_window->AcquireNextSwapchainTexture(acquireSemaphore);

	if (m_window->IsSwapchainOutOfDate())
	{
		renderer::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
		renderer::Renderer::IncrementReleaseSemaphoreToCurrentFrame();
		return;
	}

	renderer::CommandsRecordingInfo recordingInfo;
	recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("TransferCmdBuffer");
	recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	lib::UniquePtr<renderer::CommandsRecorder> recorder = renderer::Renderer::StartRecordingCommands(recordingInfo);

	recorder->StartRecording(rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	{
		renderer::Barrier barrier = renderer::RendererBuilder::CreateBarrier();
		const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
		barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::ColorRenderTarget);

		recorder->ExecuteBarrier(barrier);
	}

	rhi::TextureViewDefinition viewDefinition;
	viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	const lib::SharedPtr<renderer::TextureView> swapchainTextureView = swapchainTexture->CreateView(RENDERER_RESOURCE_NAME("TextureRenderView"), viewDefinition);

	renderer::RenderingDefinition renderingDef(rhi::ERenderingFlags::None, math::Vector2i(0, 0), m_window->GetSwapchainSize());
	renderer::RTDefinition renderTarget;
	renderTarget.textureView			= swapchainTextureView;
	renderTarget.loadOperation			= rhi::ERTLoadOperation::Clear;
	renderTarget.storeOperation			= rhi::ERTStoreOperation::Store;
	renderTarget.clearColor.asFloat[0]	= 0.f;
	renderTarget.clearColor.asFloat[1]	= 0.f;
	renderTarget.clearColor.asFloat[2]	= 0.f;
	renderTarget.clearColor.asFloat[3]	= 1.f;

	renderingDef.AddColorRenderTarget(renderTarget);

	recorder->BeginRendering(renderingDef);

	recorder->RenderUI(uiBackend);

	recorder->EndRendering();

	{
		renderer::Barrier barrier = renderer::RendererBuilder::CreateBarrier();
		const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
		barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::PresentSource);

		recorder->ExecuteBarrier(barrier);
	}

	recorder->FinishRecording();

	lib::SharedPtr<renderer::Semaphore> finishCommandsSemaphore = renderer::RendererBuilder::CreateSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);

	lib::DynamicArray<renderer::CommandsSubmitBatch> submitBatches;
	renderer::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(renderer::CommandsSubmitBatch());
	submitBatch.recordedCommands.emplace_back(std::move(recorder));
	submitBatch.waitSemaphores.AddBinarySemaphore(acquireSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitBatch.signalSemaphores.AddBinarySemaphore(finishCommandsSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitBatch.signalSemaphores.AddTimelineSemaphore(renderer::Renderer::GetReleaseFrameSemaphore(), renderer::Renderer::GetCurrentFrameIdx(), rhi::EPipelineStage::TopOfPipe);

	renderer::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

	m_window->PresentTexture({ finishCommandsSemaphore });

	if (m_window->IsSwapchainOutOfDate())
	{
		renderer::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
	}
}

}
