#pragma once

#include "UILayers/UIView.h"
#include "TextureInspectorTypes.h"
#include "UITypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg::capture
{

struct RGTextureCapture;
class TextureInspectorRenderer;
class LiveTextureOutput;
struct RGCapture;


class TextureInspector : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	TextureInspector(const scui::ViewDefinition& definition, lib::SharedRef<RGCapture> capture, const RGTextureCapture& textureCapture);
	virtual ~TextureInspector();

	// Begin UIView overrides
	virtual void             BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void             DrawUI() override;
	virtual ImGuiWindowFlags GetWindowFlags() const override;
	// End UIView overrides

private:

	void DrawTextureDetails();
	void DrawTextureViewPanel();
	void DrawTextureViewSettingPanel();
	void DrawTextureAdditionalInfoPanel();

	ui::TextureID RenderDisplayedTexture(const lib::SharedRef<rdr::TextureView>& inputTexture, math::Vector2i hoveredPixel);

	void CreateDisplayedTexture(const math::Vector2u& resolution);

	void UpdateDisplayedTexture(const lib::SharedRef<rdr::TextureView> capturedTexture);

	void OnLiveCaptureEnabled();
	void OnLiveCaptureDisabled();

	lib::SharedRef<rdr::TextureView> GetInspectedTexture() const;

	lib::HashedString m_textureDetailsPanelName;
	lib::HashedString m_textureViewPanelName;
	lib::HashedString m_textureViewSettingsPanelName;
	
	lib::HashedString m_textureAdditionalInfoPanelName;

	const RGTextureCapture& m_textureCapture;

	lib::SharedRef<RGCapture> m_capture;

	lib::SharedRef<rdr::TextureView> m_capturedTexture;

	lib::SharedPtr<rdr::TextureView> m_displayTexture;
	lib::SharedPtr<TextureInspectorRenderer> m_renderer;

	lib::SharedRef<TextureInspectorReadback> m_readback;

	TextureInspectorFilterParams m_viewParameters;

	std::optional<SaveTextureParams> m_saveParams;

	lib::SharedPtr<TextureHistogram> m_histogram;
	Int32 m_histogramVisualizationChannel;

	math::Vector2f m_viewportMin;
	Real32 m_zoom;
	Real32 m_zoomSpeed;

	Bool m_liveCaptureEnabled;
	lib::SharedPtr<LiveTextureOutput> m_liveCaptureTexture;
	lib::DelegateHandle m_liveCaptureDelegateHandle;
};

} // spt::rg::capture