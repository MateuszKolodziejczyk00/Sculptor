#pragma once

#include "TerrainEditorMacros.h"
#include "ViewportInterface.h"
#include "UILayers/UIView.h"
#include "TerrainAsset.h"
#include "TerrainEditorTypes.h"


namespace spt::rsc
{
struct SceneRendererSettings;
} // spt::rsc


namespace spt::ed
{

class ViewportInterface;


class TERRAIN_EDITOR_API TerrainEditorUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit TerrainEditorUIView(const scui::ViewDefinition& definition, const ViewportInterface& viewportInterface, const as::TerrainAssetHandle& terrainAsset = nullptr);

	void SetTerrainAsset(const as::TerrainAssetHandle& terrainAsset);
	const as::TerrainAssetHandle& GetTerrainAsset() const;

	void SetMaterialMapEditingEnabled(Bool isEnabled);
	Bool IsMaterialMapEditingEnabled() const;

protected:

	void OnTerrainAssetChanged(const as::TerrainAssetHandle& newTerrainAsset);

	//~ Begin UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void DrawUI() override;
	//~ End UIView overrides

private:

	lib::HashedString        m_windowName;
	const ViewportInterface& m_viewportInterface;

	as::TerrainAssetHandle m_terrainAsset;

	Bool                   m_isMaterialMapEditingEnabled = false;
	Int32                  m_materialIDToPaint = 1u;
	Real32                 m_brushSize = 20.f;
	Bool                   m_isPainting = false;

	lib::SharedPtr<rdr::TextureView>                    m_paintedMaterialMap;
	js::JobWithResult<lib::SharedPtr<rdr::TextureView>> m_paintedMaterialMapLoadJob;

	js::Job m_terainSaveJob;
};

} // spt::ed
