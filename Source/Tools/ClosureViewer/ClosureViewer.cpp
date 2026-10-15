#include "ImGuiApplication.h"
#include "ScopedEngine.h"
#include "ClosureViewerCommon.h"
#include "ReferenceTool.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "Types/Buffer.h"
#include "Types/UIBackend.h"
#include "ImGui/SculptorImGui.h"
#include "Utils/TransfersManager.h"
#include "Engine.h"
#include "FileSystem/File.h"


namespace spt::closure_viewer
{

namespace ui_utils
{

math::Vector2u GetAvailableTextureResolution()
{
	const ImVec2 available = ImGui::GetContentRegionAvail();
	return math::Vector2u(static_cast<Uint32>(std::max(available.x, 16.f)), static_cast<Uint32>(std::max(available.y, 16.f)));
}

} // ui_utils

class ClosureViewer : public tools::ImGuiApplication
{
protected:

	using Super = tools::ImGuiApplication;

public:

	ClosureViewer();

protected:

	// Begin tools::ImGuiApplication overrides
	virtual void DrawUI() override;
	virtual void RenderFrame(rg::RenderGraphBuilder& graphBuilder) override;
	// End tools::ImGuiApplication overrides

private:

	Bool DrawMaterialUI();
	void DrawClosure3DUI();

	void DrawFurnanceTestPanel();
	void DrawClosure3DPanel();
	void DrawClosure2DPanel();
	void DrawClosureGraphPanel();

	void UpdateView();

	lib::SharedPtr<rdr::TextureView> m_furnanceTestTexture;
	lib::SharedPtr<rdr::TextureView> m_closure3DTexture;
	lib::SharedPtr<rdr::TextureView> m_closure2DTexture;
	lib::SharedPtr<rdr::TextureView> m_closureGraphTexture;

	Real32 m_viewElevation = pi<Real32> * 0.25f;

	Real32 m_view3DPhi      = -pi<Real32> * 0.5f;
	Real32 m_view3DTheta    = pi<Real32> * 0.1f;
	Real32 m_view3DDistance = 3.f;
	Bool   m_view3DDirty    = true;

	char m_closureRefName[64] = "Reference";

	ClosureViewerData m_viewerData;
};


ClosureViewer::ClosureViewer()
	: Super("ClosureViewer")
{
	const Uint32 latitudeSegments  = 200u;
	const Uint32 longitudeSegments = 200u;
	const lib::DynamicArray<math::Vector3f> closure3DVertices = renderer::utils::GenerateDebugSphereVertices(latitudeSegments, longitudeSegments);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = closure3DVertices.size() * sizeof(math::Vector3f);
	bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	lib::SharedRef<rdr::Buffer> closure3DVerticesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Closure 3D Vertices"), bufferDef, rhi::EMemoryUsage::GPUOnly);

	rdr::GPUApi::GetTransfersManager().EnqueueUpload(closure3DVerticesBuffer, 0u, reinterpret_cast<const Byte*>(closure3DVertices.data()), bufferDef.size);

	m_viewerData.material.baseColor          = math::Vector3f::Constant(1.f);
	m_viewerData.material.metallic           = 0.f;
	m_viewerData.material.roughness          = 0.5f;
	m_viewerData.material.clearCoatWeight    = 0.f;
	m_viewerData.material.clearCoatRoughness = 0.1f;

	m_viewerData.closureScale   = 1.f;
	m_viewerData.closureOpacity = 0.5f;

	m_viewerData.frameIdx             = 0u;
	m_viewerData.closure3DVertices    = closure3DVerticesBuffer->GetFullView();
	m_viewerData.closure3DVerticesNum = static_cast<Uint32>(closure3DVertices.size());

	UpdateView();
}

void ClosureViewer::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	ImGui::Begin("Settings");

	if (ImGui::Button("Capture Render Graph"))
	{
		RequestRenderGraphCapture();
	}

	if (ImGui::Button("HotReload Shaders"))
	{
		rdr::GPUApi::HotReloadShaders();
	}

	ImGui::InputText("Reference Name", m_closureRefName, sizeof(m_closureRefName));

	if (ImGui::Button("Create Reference"))
	{
		const lib::Path outputDirectory = engn::Engine::Get().GetPaths().savedPath / ref_tool::refsDirectoryName / m_closureRefName;
		ref_tool::CreateReference(m_viewerData.material, outputDirectory);
	}

	if (DrawMaterialUI())
	{
		m_viewerData.frameIdx = 0u;
	}

	DrawClosure3DUI();

	ImGui::End();

	ImGui::Begin("Closure");

	const Real32 panelWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

	ImGui::BeginChild("Left Column", ImVec2(panelWidth, 0.f), false);
	{
		const Real32 panelHeight = (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y) * 0.5f;

		ImGui::BeginChild("Furnance Test Panel", ImVec2(0.f, panelHeight), true);
		DrawFurnanceTestPanel();
		ImGui::EndChild();

		ImGui::BeginChild("Closure 2D", ImVec2(0.f, 0.f), true);
		DrawClosure2DPanel();
		ImGui::EndChild();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Right Column", ImVec2(0.f, 0.f), false);
	{
		const Real32 panelHeight = (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y) * 0.5f;

		ImGui::BeginChild("Closure 3D", ImVec2(0.f, panelHeight), true);
		DrawClosure3DPanel();
		ImGui::EndChild();

		ImGui::BeginChild("Closure Graph Panel", ImVec2(0.f, 0.f), true);
		DrawClosureGraphPanel();
		ImGui::EndChild();
	}
	ImGui::EndChild();

	ImGui::End();
}

void ClosureViewer::DrawFurnanceTestPanel()
{
	ImGui::TextUnformatted("Furnance Test");

	const math::Vector2u resolution = ui_utils::GetAvailableTextureResolution();
	if (!m_furnanceTestTexture || resolution != m_furnanceTestTexture->GetResolution2D())
	{
		m_furnanceTestTexture = rdr::ResourcesManager::CreateTextureView2D(RENDERER_RESOURCE_NAME("Furnance Test Texture"), resolution, rhi::EFragmentFormat::RGBA32_S_Float, rhi::ETextureUsage::UAV);
		m_viewerData.frameIdx = 0u;
	}

	const math::Vector2f imageSize = resolution.cast<Real32>();
	ImGui::Image(rdr::UIBackend::GetUITextureID(lib::Ref(m_furnanceTestTexture)), imageSize);
}

void ClosureViewer::DrawClosure3DPanel()
{
	ImGui::TextUnformatted("Closure 3D");

	if (ImGui::IsKeyDown(ImGuiKey_MouseLeft) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
	{
		const ImVec2 mousDrag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.f);
		ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);

		m_view3DPhi   += mousDrag.x * 0.005f;
		m_view3DTheta += mousDrag.y * 0.005f;

		m_view3DDirty = true;
	}

	const Real32 mouseWheel = ImGui::GetIO().MouseWheel;
	if (mouseWheel != 0.f)
	{
		m_view3DDistance *= std::pow(0.9f, mouseWheel);
		m_view3DDirty     = true;
	}

	const math::Vector2u resolution = ui_utils::GetAvailableTextureResolution();
	if (!m_closure3DTexture || resolution != m_closure3DTexture->GetResolution2D())
	{
		m_closure3DTexture = rdr::ResourcesManager::CreateTextureView2D(RENDERER_RESOURCE_NAME("Closure 3D Texture"), resolution, rhi::EFragmentFormat::RGBA8_UN_Float, rhi::ETextureUsage::ColorRTV);
		UpdateView();
	}

	const math::Vector2f imageSize = resolution.cast<Real32>();
	ImGui::Image(rdr::UIBackend::GetUITextureID(lib::Ref(m_closure3DTexture)), imageSize);
}

void ClosureViewer::DrawClosure2DPanel()
{
	ImGui::TextUnformatted("Closure 2D");

	const math::Vector2u resolution = ui_utils::GetAvailableTextureResolution();
	if (!m_closure2DTexture || resolution != m_closure2DTexture->GetResolution2D())
	{
		m_closure2DTexture = rdr::ResourcesManager::CreateTextureView2D(RENDERER_RESOURCE_NAME("Closure 2D Texture"), resolution, rhi::EFragmentFormat::RGBA8_UN_Float, rhi::ETextureUsage::UAV);
	}

	const math::Vector2f imageSize = resolution.cast<Real32>();
	ImGui::Image(rdr::UIBackend::GetUITextureID(lib::Ref(m_closure2DTexture)), imageSize);
}

void ClosureViewer::DrawClosureGraphPanel()
{
	ImGui::TextUnformatted("Closure Graph");

	const math::Vector2u resolution = ui_utils::GetAvailableTextureResolution();
	if (!m_closureGraphTexture || resolution != m_closureGraphTexture->GetResolution2D())
	{
		m_closureGraphTexture = rdr::ResourcesManager::CreateTextureView2D(RENDERER_RESOURCE_NAME("Closure Graph Texture"), resolution, rhi::EFragmentFormat::RGBA8_UN_Float, rhi::ETextureUsage::UAV);
	}

	const math::Vector2f imageSize = resolution.cast<Real32>();
	ImGui::Image(rdr::UIBackend::GetUITextureID(lib::Ref(m_closureGraphTexture)), imageSize);
}

void ClosureViewer::RenderFrame(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	renderer::RenderFurnanceTest(graphBuilder, graphBuilder.AcquireExternalTextureView(m_furnanceTestTexture), m_viewerData);

	renderer::RenderClosure3D(graphBuilder, graphBuilder.AcquireExternalTextureView(m_closure3DTexture), m_viewerData);

	renderer::RenderClosure2D(graphBuilder, graphBuilder.AcquireExternalTextureView(m_closure2DTexture), m_viewerData);

	renderer::RenderClosureGraph(graphBuilder, graphBuilder.AcquireExternalTextureView(m_closureGraphTexture), m_viewerData);

	++m_viewerData.frameIdx;
}

Bool ClosureViewer::DrawMaterialUI()
{
	ClosureMaterial& material = m_viewerData.material;

	Bool changed = false;

	ImGui::Separator();
	ImGui::TextUnformatted("Material");
	changed |= ImGui::ColorEdit3("Base Color", material.baseColor.data());
	changed |= ImGui::SliderFloat("Metallic", &material.metallic, 0.f, 1.f);
	changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.f, 1.f);
	changed |= ImGui::SliderFloat("Clear Coat Weight", &material.clearCoatWeight, 0.f, 1.f);
	changed |= ImGui::SliderFloat("Clear Coat Roughness", &material.clearCoatRoughness, 0.f, 1.f);

	return changed;
}

void ClosureViewer::DrawClosure3DUI()
{
	ImGui::Separator();
	ImGui::TextUnformatted("Closure 3D");

	if (ImGui::SliderAngle("View Elevation", &m_viewElevation, 0.f, 90.f) || m_view3DDirty)
	{
		UpdateView();
		m_view3DDirty = false;
	}

	ImGui::SliderFloat("Closure Scale", &m_viewerData.closureScale, 0.01f, 10.f, "%.3f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Closure Opacity", &m_viewerData.closureOpacity, 0.f, 1.f);

	ImGui::TextColored(ImVec4(1.f, 0.85f, 0.1f, 1.f), "View");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Mirror");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.f, 1.f), "Normal");
}

void ClosureViewer::UpdateView()
{
	m_viewerData.viewDirection = math::Vector3f(std::cos(m_viewElevation), 0.f, std::sin(m_viewElevation));

	const math::Vector3f cameraLocation = math::Vector3f(m_view3DDistance * std::cos(m_view3DPhi) * std::cos(m_view3DTheta),
														 m_view3DDistance * std::sin(m_view3DPhi) * std::cos(m_view3DTheta),
														 m_view3DDistance * std::sin(m_view3DTheta));

	const math::Vector3f cameraTarget   = math::Vector3f(0.f, 0.f, 0.4f);

	const math::Vector2f resolution = m_closure3DTexture ? m_closure3DTexture->GetResolution2D().cast<Real32>() : math::Vector2f(1.f, 1.f);

	const math::Matrix4f projectionMatrix = renderer::utils::CreatePerspectiveProjection(pi<Real32> * 0.25f, resolution, 0.01f);
	const math::Matrix4f viewMatrix       = renderer::utils::CreateLookAtMatrix(cameraLocation, cameraTarget);

	m_viewerData.viewProjectionMatrix = projectionMatrix * viewMatrix;
}

} // spt::closure_viewer


int main()
{
	using namespace spt;

	const tools::ScopedEngine engine;

	const engn::CommandLineArguments& cmdLineArgs = engn::Engine::Get().GetCmdLineArgs();

	if (cmdLineArgs.Contains("-Ref"))
	{
		const lib::Path referenceDirectory = engn::Engine::Get().GetPaths().savedPath / closure_viewer::ref_tool::refsDirectoryName / cmdLineArgs.GetValue("-Ref").ToString();
		const lib::Path raportPath = engn::Engine::Get().GetPaths().savedPath / closure_viewer::ref_tool::raportsDirectoryName / (lib::File::Utils::CreateFileNameFromTime() + ".html");

		const Bool success = closure_viewer::ref_tool::GenerateComparisonRaport(referenceDirectory, raportPath);

		return success ? 0 : 1;
	}
	else
	{
		closure_viewer::ClosureViewer app;
		app.Run();
	}

	return 0;
}
