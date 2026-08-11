#pragma once

#include "CloudscapeEditorMacros.h"
#include "UILayers/UIView.h"
#include "CloudscapeAsset.h"

#include <array>


namespace spt::ed
{

class ViewportInterface;


class CLOUDSCAPE_EDITOR_API CloudscapeEditorUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit CloudscapeEditorUIView(const scui::ViewDefinition& definition, const ViewportInterface& viewportInterface, const as::CloudscapeAssetHandle& cloudscapeAsset = nullptr);

	void SetCloudscapeAsset(const as::CloudscapeAssetHandle& cloudscapeAsset);
	const as::CloudscapeAssetHandle& GetCloudscapeAsset() const;

	void SetWeatherMapEditingEnabled(Bool isEnabled);

protected:

	//~ Begin UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void DrawUI() override;
	//~ End UIView overrides

private:

	enum EWeatherMapChannel : Uint8
	{
		Coverage = 0,
		Type     = 1,
		B        = 2,
		A        = 3,

		Count
	};

	enum class EWeatherMapPaintMode : Uint8
	{
		None,
		Paint,
		Erase,
	};

	lib::HashedString m_windowName;
	const ViewportInterface& m_viewportInterface;

	as::CloudscapeAssetHandle m_cloudscapeAsset;

	Bool  m_isWeatherMapEditingEnabled = false;

	EWeatherMapChannel m_weatherChannelToPaint = EWeatherMapChannel::Coverage;

	Real32 m_brushSize                 = 512.f;
	Real32 m_brushFalloff              = 0.5f;
	Real32 m_paintValue                = 1.f;

	EWeatherMapPaintMode m_paintMode = EWeatherMapPaintMode::None;

	lib::SharedPtr<rdr::TextureView> m_paintedWeatherMap;

	js::JobWithResult<lib::SharedPtr<rdr::TextureView>> m_paintedWeatherMapLoadJob;
	js::Job                                             m_cloudscapeSaveJob;
};

} // spt::ed
