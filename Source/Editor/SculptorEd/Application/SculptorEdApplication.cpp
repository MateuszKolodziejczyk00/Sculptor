#include "SculptorEdApplication.h"
#include "Renderer.h"
#include "ResourcesManager.h"
#include "Timer/TickingTimer.h"
#include "Types/Semaphore.h"
#include "Types/Texture.h"
#include "Types/Context.h"
#include "CommandsRecorder/CommandsRecorder.h"
#include "CommandsRecorder/RenderingDefinition.h"
#include "UIContextManager.h"
#include "Profiler.h"
#include "Profiler/GPUProfiler.h"
#include "Engine.h"
#include "Types/DescriptorSetState.h"
#include "Common/ShaderCompilationInput.h"
#include "ImGui/SculptorImGui.h"


namespace spt::ed
{

class StorageTextureBinding : public rdr::DescriptorSetBinding
{
public:

	explicit StorageTextureBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag)
		: rdr::DescriptorSetBinding(name, descriptorDirtyFlag)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateTexture(GetName(), texture);
	}

	virtual void CreateBindingMetaData(OUT smd::GenericShaderBinding& binding) const final
	{
		binding.Set(smd::TextureBindingData(1, smd::EBindingFlags::Storage));
	}

	void Set(const lib::SharedPtr<rdr::TextureView>& inTexture)
	{
		texture = inTexture;
		MarkAsDirty();
	}

	void Reset()
	{
		texture.reset();
	}

private:

	lib::SharedPtr<rdr::TextureView> texture;
};

DS_BEGIN(, TestDS, rhi::EShaderStageFlags::Compute)
DS_BINDING(StorageTextureBinding, u_texture)
DS_END()

lib::SharedPtr<TestDS> ds;
lib::SharedPtr<rdr::Texture> texture;
rdr::PipelineStateID computePipelineID;

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

	rdr::Renderer::Initialize();

	m_window = rdr::ResourcesManager::CreateWindow("SculptorEd", math::Vector2u(1920, 1080));

	rdr::Renderer::PostCreatedWindow();

	const rdr::ShaderID shaderID = rdr::ResourcesManager::CreateShader("Test.hlsl", sc::ShaderCompilationSettings());
	computePipelineID = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("CompTest"), shaderID);
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

	uiBackend = rdr::ResourcesManager::CreateUIBackend(context, m_window);

	ImGui::SetCurrentContext(context.GetHandle());

	{
		lib::UniquePtr<rdr::CommandsRecorder> recorder = rdr::Renderer::StartRecordingCommands();

		recorder->InitializeUIFonts(uiBackend);

		const lib::SharedRef<rdr::Context> renderingContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("InitUIContext"), rhi::ContextDefinition());

		rdr::CommandsRecordingInfo recordingInfo;
		recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("InitializeUICommandBuffer");
		recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		recorder->RecordCommands(renderingContext, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

		lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
		rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
		submitBatch.recordedCommands.emplace_back(std::move(recorder));

		rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

		rdr::Renderer::WaitIdle();

		uiBackend->DestroyFontsTemporaryObjects();
	}

	lib::TickingTimer timer;

	{
		rhi::TextureDefinition textureDef;
		textureDef.resolution = math::Vector3u(1920, 1080, 1);
		textureDef.usage = rhi::ETextureUsage::StorageTexture;

		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange.aspect = rhi::ETextureAspect::Color;

		ds = lib::MakeShared<TestDS>(rdr::EDescriptorSetStateFlags::Persistent);

		texture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("TestTexture"), textureDef, rhi::RHIAllocationInfo());
		ds->u_texture.Set(texture->CreateView(RENDERER_RESOURCE_NAME("TestTextureView"), viewDef));
	}

	while (true)
	{
		SPT_PROFILER_FRAME("EditorFrame");

		const Real32 deltaTime = timer.Tick();

		m_window->Update(deltaTime);

		if (m_window->ShouldClose())
		{
			break;
		}

		rdr::Renderer::BeginFrame();

		m_window->BeginFrame();

		uiBackend->BeginFrame();

		ImGui::NewFrame();

		ImGui::Begin("ProfilerTest");
		if (prf::Profiler::StartedCapture())
		{
			if (ImGui::Button("Stop Trace"))
			{
				prf::Profiler::StopCapture();
				prf::Profiler::SaveCapture();
			}
		}
		else
		{
			if (ImGui::Button("Start Trace"))
			{
				prf::Profiler::StartCapture();
			}
		}
		ImGui::End();

		ImGui::Begin("ASD");
		ImGui::Text("TESTSDasd");
		ImGui::End();

		ImGui::Render();
		
		if (imGuiIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			// Disable warnings, so that they are not spamming when ImGui backend allocates memory not from pools
			rdr::Renderer::EnableValidationWarnings(false);
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			rdr::Renderer::EnableValidationWarnings(true);
		}

		RenderFrame();

		rdr::Renderer::EndFrame();
	}
}

void SculptorEdApplication::OnShutdown()
{
	Super::OnShutdown();

	ds->u_texture.Reset();
	texture.reset();

	uiBackend.reset();

	rdr::Renderer::WaitIdle();
	
	m_window->UninitializeUI();

	m_window.reset();

	rdr::Renderer::Uninitialize();
}

void SculptorEdApplication::RenderFrame()
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

	const lib::SharedRef<rdr::Context> renderingContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("MainThreadContext"), rhi::ContextDefinition());

	lib::UniquePtr<rdr::CommandsRecorder> recorder = rdr::Renderer::StartRecordingCommands();

	{
		SPT_GPU_PROFILER_CONTEXT(recorder->GetCommandsBuffer());

		{
			rdr::Barrier barrier = rdr::ResourcesManager::CreateBarrier();
			const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::ColorRenderTarget);

			recorder->ExecuteBarrier(std::move(barrier));
		}

		{
			SPT_GPU_PROFILER_EVENT("TestCompute");

			rdr::Barrier barrier = rdr::ResourcesManager::CreateBarrier();
			const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(texture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::ComputeGeneral);

			recorder->ExecuteBarrier(std::move(barrier));

			recorder->BindComputePipeline(computePipelineID);

			recorder->BindDescriptorSetState(ds);

			recorder->Dispatch(math::Vector3u(1, 1, 1));

			recorder->UnbindDescriptorSetState(ds);
		}

		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		const lib::SharedRef<rdr::TextureView> swapchainTextureView = swapchainTexture->CreateView(RENDERER_RESOURCE_NAME("TextureRenderView"), viewDefinition);

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

			recorder->BeginRendering(renderingDef);

			recorder->RenderUI(uiBackend);

			recorder->EndRendering();
		}

		{
			rdr::Barrier barrier = rdr::ResourcesManager::CreateBarrier();
			const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::PresentSource);

			recorder->ExecuteBarrier(std::move(barrier));
		}
	}

	rdr::CommandsRecordingInfo recordingInfo;
	recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("TransferCmdBuffer");
	recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	recorder->RecordCommands(renderingContext, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	lib::SharedRef<rdr::Semaphore> finishCommandsSemaphore = rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);

	lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
	rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
	submitBatch.recordedCommands.emplace_back(std::move(recorder));
	submitBatch.waitSemaphores.AddBinarySemaphore(acquireSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitBatch.signalSemaphores.AddBinarySemaphore(finishCommandsSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitBatch.signalSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), rdr::Renderer::GetCurrentFrameIdx(), rhi::EPipelineStage::TopOfPipe);

	rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

	rdr::Renderer::PresentTexture(lib::ToSharedRef(m_window), { finishCommandsSemaphore });

	if (m_window->IsSwapchainOutOfDate())
	{
		rdr::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
	}
}

}
