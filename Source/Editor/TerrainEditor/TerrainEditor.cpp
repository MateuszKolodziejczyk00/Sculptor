#include "TerrainEditor.h"
#include "ImGui/DockBuilder.h"
#include "JobSystem.h"
#include "SceneRenderer/SceneRenderer.h"
#include "EditorFrame.h"
#include "UIElements/ApplicationUI.h"
#include "Loaders/TextureLoader.h"
#include "InputManager.h"
#include "ViewportInterface.h"


namespace spt::ed
{

TerrainEditorUIView::TerrainEditorUIView(const scui::ViewDefinition& definition, const ViewportInterface& viewportInterface, const as::TerrainAssetHandle& terrainAsset /* = nullptr */)
	: Super(definition)
	, m_windowName(CreateUniqueName("Terrain Editor"))
	, m_viewportInterface(viewportInterface)
	, m_terrainAsset(terrainAsset)
{
	OnTerrainAssetChanged(m_terrainAsset);
}

void TerrainEditorUIView::SetTerrainAsset(const as::TerrainAssetHandle& terrainAsset)
{
	m_terrainAsset = terrainAsset;
}

const as::TerrainAssetHandle& TerrainEditorUIView::GetTerrainAsset() const
{
	return m_terrainAsset;
}

void TerrainEditorUIView::SetMaterialMapEditingEnabled(Bool isEnabled)
{
	m_isMaterialMapEditingEnabled = isEnabled;
}

Bool TerrainEditorUIView::IsMaterialMapEditingEnabled() const
{
	return m_isMaterialMapEditingEnabled;
}

void TerrainEditorUIView::OnTerrainAssetChanged(const as::TerrainAssetHandle& newTerrainAsset)
{
	SPT_PROFILER_FUNCTION();

	if (!newTerrainAsset.IsValid())
	{
		return;
	}

	const as::TerrainAssetDefinition& terrainAssetDef = newTerrainAsset->GetTerrainAssetDefinition();
	if (!terrainAssetDef.materialIDsTex.empty())
	{
		const lib::Path materialIDsPath = m_terrainAsset->GetDirectoryPath() / terrainAssetDef.materialIDsTex;
		if (!lib::File::Exists(materialIDsPath))
		{
			const math::Vector3u resolution = math::Vector3u(2048u, 2048u, 1u);
			const rhi::EFragmentFormat format = rhi::EFragmentFormat::R8_U_Int;

			const lib::DynamicArray<Byte> emptyData(resolution.x() * resolution.y() * resolution.z(), Byte(0u));

			gfx::TextureWriter::SaveTexture(resolution, format, emptyData, materialIDsPath.generic_string());
		}
	}
}

void TerrainEditorUIView::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID, ui::DockWindow(m_windowName));
}

void TerrainEditorUIView::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_windowName.GetData());

	EditorFrameContext& editorFrameContext = static_cast<EditorFrameContext&>(scui::ApplicationUI::GetCurrentContext().GetCurrentFrame());

	editorFrameContext.terrainEditorState = editorFrameContext.GetFrameMemoryArena().AllocateType<TerrainEditorFrameState>();
	TerrainEditorFrameState& edFrameState = *editorFrameContext.terrainEditorState;

	editorFrameContext.terrainEditorStateDeleter = [](TerrainEditorFrameState* state)
	{
		state->~TerrainEditorFrameState();
	};

	Super::DrawUI();

	if (m_terrainAsset.IsValid())
	{
		ImGui::Text("Terrain Asset: %s", m_terrainAsset->GetName().GetData());
	}
	else
	{
		ImGui::TextUnformatted("Terrain Asset: <None>");
	}

	if (m_terainSaveJob.IsFinished())
	{
		m_terainSaveJob.Reset();

		// Post save
		m_paintedMaterialMap.reset();
	}

	if (m_paintedMaterialMapLoadJob.IsFinished())
	{
		SPT_CHECK(!m_paintedMaterialMap);
		m_paintedMaterialMap = m_paintedMaterialMapLoadJob.GetResult();
		m_paintedMaterialMapLoadJob.Reset();
	}

	const Bool isSavingTerrainAsset = m_terainSaveJob.IsValid();
	const Bool isLoadingMaterialMap = m_paintedMaterialMapLoadJob.IsValid();

	const Bool isLoadingAnything = isLoadingMaterialMap;

	const Bool canEdit = !isSavingTerrainAsset && !isLoadingAnything;

	if (canEdit)
	{
		if (ImGui::Button("Save Terrain Asset"))
		{
			m_isMaterialMapEditingEnabled = false;

			m_terainSaveJob = js::Launch(SPT_GENERIC_JOB_NAME, 
										[this]()
										{
											if (m_paintedMaterialMap)
											{
												const as::TerrainAssetDefinition& terrainAssetDef = m_terrainAsset->GetTerrainAssetDefinition();
												lib::Path materialIDsPath = m_terrainAsset->GetDirectoryPath() / terrainAssetDef.materialIDsTex;
												gfx::TextureWriter::SaveTexture(m_paintedMaterialMap->GetTexture(), materialIDsPath.generic_string());
											}
							
											m_terrainAsset->SaveAsset();
										});
		}

		if (ImGui::Checkbox("Enable Material Map Editing", &m_isMaterialMapEditingEnabled))
		{
			if (m_isMaterialMapEditingEnabled && !m_paintedMaterialMap)
			{
				const as::TerrainAssetDefinition& terrainAssetDef = m_terrainAsset->GetTerrainAssetDefinition();
				const lib::Path materialIDsPath = m_terrainAsset->GetDirectoryPath() / terrainAssetDef.materialIDsTex;

				m_paintedMaterialMapLoadJob = js::Launch(SPT_GENERIC_JOB_NAME,
												  [materialIDsPath]() -> lib::SharedPtr<rdr::TextureView>
												  {
													  gfx::TextureLoadParams loadParams;
													  loadParams.usage       = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::StorageTexture);
													  loadParams.memoryUsage = rhi::EMemoryUsage::GPUToCpu;
													  loadParams.forceTiling = rhi::ETextureTiling::Linear;
													  const lib::SharedPtr<rdr::Texture> paintedMaterialMapTexture = gfx::TextureLoader::LoadTexture(materialIDsPath.generic_string(), loadParams);
													  return paintedMaterialMapTexture->CreateView(RENDERER_RESOURCE_NAME("Painted Material Map TextureView"));
												  });
			}
		}

		ImGui::Separator();

		if (m_isMaterialMapEditingEnabled)
		{
			ImGui::SliderFloat("Brush Size", &m_brushSize, 1.f, 500.f);

			rsc::editor::TerrainInfluenceGizmo gizmo;
			gizmo.radius  = m_brushSize;
			gizmo.color   = math::Vector3f(0.f, 1.f, 0.f);
			gizmo.opacity = 0.7f;
			edFrameState.activeGizmo = gizmo;

			const as::TerrainMaterialAssetHandle terrainMaterial = m_terrainAsset->GetTerrainMaterialAsset();
			const lib::Span<const as::MaterialAssetHandle> materialAssets = terrainMaterial->GetMaterialAssets();

			ImGui::Separator();

			ImGui::Text("Select Material to Paint:");

			for (Uint32 i = 0; i < materialAssets.size(); ++i)
			{
				const as::MaterialAssetHandle materialAsset = materialAssets[i];

				const lib::HashedString materialName = materialAsset.IsValid() ? materialAsset->GetName() : lib::HashedString("Invalid Material");

				if (ImGui::Selectable(materialName.GetData(), m_materialIDToPaint == static_cast<Int32>(i)))
				{
					m_materialIDToPaint = static_cast<Int32>(i);
				}
			}

			ImGui::Separator();

			if (!inp::InputManager::Get().IsKeyPressed(inp::EKey::LeftMouseButton))
			{
				m_isPainting = false;
			}

			if (m_viewportInterface.IsFocused() && inp::InputManager::Get().WasKeyJustPressed(inp::EKey::LeftMouseButton))
			{
				m_isPainting = true;
			}

			if (m_isPainting)
			{
				edFrameState.materialPaintCommand = rsc::editor::TerrainMaterialMapPaintCommand{ .materialIDToPaint = static_cast<Uint32>(m_materialIDToPaint) };
			}
		}
	}
	else
	{
		if (isSavingTerrainAsset)
		{
			ImGui::Text("Saving Terrain Asset...");
		}
		else if (isLoadingMaterialMap)
		{
			ImGui::Text("Loading Material Map...");
		}
	}

	if (!isLoadingMaterialMap && m_paintedMaterialMap)
	{
		edFrameState.paintedMaterialMap = m_paintedMaterialMap;
	}

	ImGui::End();
}

} // spt::ed
