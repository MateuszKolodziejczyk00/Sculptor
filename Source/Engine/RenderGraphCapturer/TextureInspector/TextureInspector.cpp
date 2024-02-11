#include "TextureInspector.h"
#include "RenderGraphCaptureTypes.h"
#include "Types/Texture.h"
#include "ImGui/SculptorImGui.h"
#include "ImGui/DockBuilder.h"
#include "ResourcesManager.h"
#include "JobSystem.h"
#include "EngineFrame.h"
#include "UIElements/ApplicationUI.h"
#include "TextureInspectorRenderer.h"
#include "Types/UIBackend.h"
#include "UIUtils.h"
#include "InputManager.h"

#pragma optimize("", off)
namespace spt::rg::capture
{

TextureInspector::TextureInspector(const scui::ViewDefinition& definition, const RGTextureCapture& textureCapture)
	: Super(definition)
	, m_textureDetailsPanelName(CreateUniqueName("TextureDetails"))
	, m_textureViewPanelName(CreateUniqueName("TextureView"))
	, m_textureViewSettingsPanelName(CreateUniqueName("TextureViewSettings"))
	, m_capturedTexture(textureCapture.textureView)
	, m_textureCapture(textureCapture)
	, m_readback(lib::MakeShared<TextureInspectorReadback>())
	, m_viewportMin(math::Vector2f::Zero())
	, m_zoom(1.0f)
	, m_zoomSpeed(0.075f)
{
	m_viewParameters.minValue        = 0.0f;
	m_viewParameters.maxValue        = 1.0f;
	m_viewParameters.rChannelVisible = true;
	m_viewParameters.gChannelVisible = true;
	m_viewParameters.bChannelVisible = true;
	m_viewParameters.isIntTexture    = rhi::IsIntegerFormat(m_capturedTexture->GetRHI().GetFormat());

	InitializeTextureView(textureCapture);
}

void TextureInspector::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID,
			  ui::Split(ui::ESplit::Horizontal, 0.75f,
						ui::DockWindow(m_textureViewPanelName),
						ui::Split(ui::ESplit::Vertical, 0.5f,
								  ui::DockWindow(m_textureViewSettingsPanelName),
								  ui::DockWindow(m_textureDetailsPanelName))));
}

void TextureInspector::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	DrawTextureViewSettingPanel();

	DrawTextureDetails();

	DrawTextureViewPanel();
}

ImGuiWindowFlags TextureInspector::GetWindowFlags() const
{
	return Super::GetWindowFlags()
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoScrollWithMouse;
}

void TextureInspector::DrawTextureDetails()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<rdr::TextureView>& textureView = m_capturedTexture;

	const math::Vector2u resolution = textureView->GetResolution2D();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_textureDetailsPanelName.GetData());

	ImGui::Text("Texture: %s", textureView->GetRHI().GetName().GetData());

	ImGui::Separator();

	ImGui::Text("Resolution: %u x %u", resolution.x(), resolution.y());

	ImGui::End();
}

void TextureInspector::DrawTextureViewPanel()
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_textureViewPanelName.GetData(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

	ImGui::Columns(2);
	ImGui::Text("Zoom : %f", m_zoom);
	ImGui::NextColumn();
	ImGui::SliderFloat("Zoom Speed", &m_zoomSpeed, 0.01f, 1.f);
	ImGui::EndColumns();

	const math::Vector2f panelSize = ui::UIUtils::GetWindowContentSize();
	
	const math::Vector2f windowSize = panelSize - math::Vector2f(0.f, 50.f);

	ImGui::BeginChild("TextureView", windowSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);
	const math::Vector2f textureResolution = m_capturedTexture->GetResolution2D().cast<Real32>();

	const inp::InputManager& inputManager = inp::InputManager::Get();

	const Real32 previousZoom = m_zoom;
	
	if (ImGui::IsWindowHovered())
	{
		m_zoom = std::max(0.1f, m_zoom + m_zoom * inputManager.GetScrollDelta() * m_zoomSpeed);
	}

	const math::Vector2f imagePosition  = ImGui::GetCurrentWindow()->DC.CursorPos;
	const math::Vector2f cursorOffset   = math::Vector2f(ImGui::GetMousePos()) - imagePosition;
	const math::Vector2f cursorWindowUV = cursorOffset.cwiseQuotient(windowSize).cwiseMax(math::Vector2f::Zero()).cwiseMin(math::Vector2f::Ones());

	const math::Vector2f viewportSize = windowSize / m_zoom;

	const Real32 zoomDelta = m_zoom - previousZoom;
	if (!math::Utils::IsNearlyZero(zoomDelta))
	{
		const math::Vector2f prevViewportSize = windowSize / previousZoom;
		m_viewportMin += prevViewportSize.cwiseProduct(cursorWindowUV);
		m_viewportMin -= viewportSize.cwiseProduct(cursorWindowUV);
	}

	m_viewportMin = m_viewportMin.cwiseMin(textureResolution - viewportSize).cwiseMax(math::Vector2f::Zero());
	math::Vector2f viewportMax = m_viewportMin + viewportSize;

	if (ImGui::IsWindowHovered() && inputManager.IsKeyPressed(inp::EKey::RightMouseButton))
	{
		math::Vector2f moveDelta = -inputManager.GetMouseMoveDelta().cast<Real32>().cwiseQuotient(math::Vector2f::Constant(m_zoom));

		moveDelta.x() = std::clamp<Real32>(moveDelta.x(), -m_viewportMin.x(), std::max(textureResolution.x() - viewportSize.x(), 0.f));
		moveDelta.y() = std::clamp<Real32>(moveDelta.y(), -m_viewportMin.y(), std::max(textureResolution.y() - viewportSize.y(), 0.f));

		m_viewportMin += moveDelta;
		viewportMax   += moveDelta;
	}

	const math::Vector2f uv0 = m_viewportMin.cast<Real32>().cwiseQuotient(textureResolution.cast<Real32>());
	const math::Vector2f uv1 = viewportMax.cast<Real32>().cwiseQuotient(textureResolution.cast<Real32>());

	const math::Vector2f hoveredUV    = (uv0 + (uv1 - uv0).cwiseProduct(cursorWindowUV)).cwiseMin(math::Vector2f::Ones()).cwiseMax(math::Vector2f::Zero());
	const math::Vector2i hoveredPixel = textureResolution.cwiseProduct(hoveredUV).cast<Int32>();

	if (windowSize.x() > 1.f && windowSize.y() > 1.f)
	{
		const ui::TextureID textureID = RenderDisplayedTexture(hoveredPixel);

		ImGui::Image(textureID, windowSize, uv0, uv1);
	}

	const TextureInspectorReadbackData readbackData = m_readback->GetData();
	const math::Vector4f hoveredPixelValue = readbackData.hoveredPixelValue;

	ImGui::EndChild();

	ImGui::Columns(3);
	ImGui::Text("UV: [%f, %f]", hoveredUV.x(), hoveredUV.y());
	ImGui::NextColumn();
	ImGui::Text("Pixel: [%d, %d]", hoveredPixel.x(), hoveredPixel.y());
	ImGui::NextColumn();
	ImGui::Text("Value: [R: %f, G: %f, B: %f, A: %f]", hoveredPixelValue.x(), hoveredPixelValue.y(), hoveredPixelValue.z(), hoveredPixelValue.w());
	ImGui::EndColumns();
	

	ImGui::End();
}

void TextureInspector::DrawTextureViewSettingPanel()
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_textureViewSettingsPanelName.GetData());

	ImGui::Text("View Details");

	const char* visualizationModes[] = { "Color", "Alpha" };
	ImGui::Combo("Visualization Mode", reinterpret_cast<int*>(&m_viewParameters.visualizationMode), visualizationModes, SPT_ARRAY_SIZE(visualizationModes));

	const ETextureInspectorVisualizationMode visualizationMode = static_cast<ETextureInspectorVisualizationMode>(m_viewParameters.visualizationMode);
	if (visualizationMode == ETextureInspectorVisualizationMode::Color)
	{
		ImGui::Columns(3);

		ImGui::Text("R");
		ImGui::Checkbox("##R", reinterpret_cast<Bool*>(&m_viewParameters.rChannelVisible));
		ImGui::NextColumn();

		ImGui::Text("G");
		ImGui::Checkbox("##G", reinterpret_cast<Bool*>(&m_viewParameters.gChannelVisible));
		ImGui::NextColumn();

		ImGui::Text("B");
		ImGui::Checkbox("##B", reinterpret_cast<Bool*>(&m_viewParameters.bChannelVisible));

		ImGui::EndColumns();

		ImGui::Columns(2);

		ImGui::DragFloat("Min Value", &m_viewParameters.minValue, 0.01f);
		ImGui::NextColumn();
		ImGui::DragFloat("Max Value", &m_viewParameters.maxValue, 0.01f);

		ImGui::EndColumns();
	}

	ImGui::End();
}

ui::TextureID TextureInspector::RenderDisplayedTexture(math::Vector2i hoveredPixel)
{
	SPT_PROFILER_FUNCTION();

	m_viewParameters.hoveredPixel = hoveredPixel;

	engn::FrameContext& currentFrame = scui::ApplicationUI::GetCurrentContext().GetCurrentFrame();

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [renderParams = m_viewParameters, renderer = m_renderer, readback = m_readback]
			   {
					renderer->SetParameters(renderParams);
					renderer->Render(readback);
			   },
			   js::Prerequisites(currentFrame.GetStageBeginEvent(engn::EFrameStage::ProcessViewsRendering)),
			   js::JobDef()
				   .SetPriority(js::EJobPriority::High)
				   .ExecuteBefore(currentFrame.GetStageFinishedEvent(engn::EFrameStage::ProcessViewsRendering)));

	return rdr::UIBackend::GetUITextureID(lib::Ref(m_displayTexture), rhi::ESamplerFilterType::Nearest, rhi::EMipMapAddressingMode::Nearest, rhi::EAxisAddressingMode::ClampToBorder);
}

void TextureInspector::InitializeTextureView(const RGTextureCapture& textureCapture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = textureCapture.textureView->GetResolution2D();

	rhi::TextureDefinition textureDefinition;
	textureDefinition.resolution = resolution;
	textureDefinition.usage      = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
	textureDefinition.format     = rhi::EFragmentFormat::RGBA8_UN_Float;

	m_displayTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Texture Inspector Output"), textureDefinition, rhi::EMemoryUsage::GPUOnly);

	m_renderer = lib::MakeShared<TextureInspectorRenderer>(m_capturedTexture, lib::Ref(m_displayTexture));
}

} // spt::rg::capture
