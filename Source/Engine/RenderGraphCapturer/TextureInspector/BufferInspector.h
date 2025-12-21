#pragma once

#include "UILayers/UIView.h"
#include "TextureInspectorTypes.h"
#include "UITypes.h"
#include "Delegates/MulticastDelegate.h"
#include "RenderGraphCaptureTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg::capture
{

struct CapturedBufferBinding;
struct RGCapture;
class RGNodeCaptureViewer;


struct BufferInspectParams
{
	const CapturedBuffer::Version* bufferVersion = nullptr;
	Uint64 offset = 0u;
	Uint64 size   = 0u; // 0 means full buffer

	lib::HashedString structTypeName;

	std::optional<std::pair<Uint32, Uint32>> elementsRange;
};


class BufferInspector : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	BufferInspector(const scui::ViewDefinition& definition, RGNodeCaptureViewer& parentNodeViewer, const BufferInspectParams& inspectParams);

	// Begin UIView overrides
	virtual void             BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void             DrawUI() override;
	// End UIView overrides

private:

	void DrawStruct(lib::StringView name, lib::HashedString typeName, lib::Span<const Byte> data, Uint32 currentOffset);

	lib::HashedString m_boundDataPanelName;
	lib::HashedString m_bufferDataPanelName;

	Int32 m_byteViewerOffset   = 0;
	Int32 m_dataViewerBeginidx = 0;

	BufferInspectParams m_inspectParams;

	RGNodeCaptureViewer& m_parentNodeViewer;
};

} // spt::rg::capture
