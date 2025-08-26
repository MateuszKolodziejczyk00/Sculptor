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

struct CapturedBufferBinding;
struct RGCapture;
class RGNodeCaptureViewer;


class BufferInspector : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	BufferInspector(const scui::ViewDefinition& definition, RGNodeCaptureViewer& parentNodeViewer, const CapturedBufferBinding& buffer);

	// Begin UIView overrides
	virtual void             BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void             DrawUI() override;
	// End UIView overrides

private:

	void DrawStruct(lib::StringView name, lib::HashedString typeName, lib::Span<const Byte> data);

	lib::HashedString m_boundDataPanelName;
	lib::HashedString m_bufferDataPanelName;

	const CapturedBufferBinding& m_capturedBufferBinding;

	RGNodeCaptureViewer& m_parentNodeViewer;
};

} // spt::rg::capture
