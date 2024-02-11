#pragma once

#include "UILayers/UIView.h"
#include "TextureInspectorTypes.h"
#include "UITypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg::capture
{

struct RGTextureCapture;
class TextureInspectorRenderer;


class TextureInspector : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	TextureInspector(const scui::ViewDefinition& definition, const RGTextureCapture& textureCapture);

	// Begin UIView overrides
	virtual void             BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void             DrawUI() override;
	virtual ImGuiWindowFlags GetWindowFlags() const override;
	// End UIView overrides

private:

	void DrawTextureDetails();
	void DrawTextureViewPanel();
	void DrawTextureViewSettingPanel();

	ui::TextureID RenderDisplayedTexture(math::Vector2i hoveredPixel);

	void InitializeTextureView(const RGTextureCapture& textureCapture);

	lib::HashedString m_textureDetailsPanelName;
	lib::HashedString m_textureViewPanelName;
	lib::HashedString m_textureViewSettingsPanelName;

	const RGTextureCapture& m_textureCapture;

	lib::SharedRef<rdr::TextureView> m_capturedTexture;

	lib::SharedPtr<rdr::TextureView> m_displayTexture;
	lib::SharedPtr<TextureInspectorRenderer> m_renderer;

	lib::SharedRef<TextureInspectorReadback> m_readback;

	TextureInspectorFilterParams m_viewParameters;

	math::Vector2f m_viewportMin;
	Real32 m_zoom;
	Real32 m_zoomSpeed;
};

} // spt::rg::capture