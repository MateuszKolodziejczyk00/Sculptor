#include "CloudscapeEditor.h"
#include "EditorFrame.h"
#include "ImGui/DockBuilder.h"
#include "JobSystem.h"
#include "UIElements/ApplicationUI.h"
#include "Loaders/TextureLoader.h"
#include "RHICore/RHITextureTypes.h"
#include "CloudscapeEditorTypes.h"
#include "ViewportInterface.h"
#include "InputManager.h"
#include "ResourcesManager.h"


namespace spt::ed
{

CloudscapeEditorUIView::CloudscapeEditorUIView(const scui::ViewDefinition& definition, const ViewportInterface& viewportInterface, const as::CloudscapeAssetHandle& cloudscapeAsset /* = nullptr */)
	: Super(definition)
	, m_windowName(CreateUniqueName("Cloudscape Editor"))
	, m_viewportInterface(viewportInterface)
	, m_cloudscapeAsset(cloudscapeAsset)
{
}

void CloudscapeEditorUIView::SetCloudscapeAsset(const as::CloudscapeAssetHandle& cloudscapeAsset)
{
	m_cloudscapeAsset = cloudscapeAsset;
}

const as::CloudscapeAssetHandle& CloudscapeEditorUIView::GetCloudscapeAsset() const
{
	return m_cloudscapeAsset;
}

void CloudscapeEditorUIView::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID, ui::DockWindow(m_windowName));
}

void CloudscapeEditorUIView::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_windowName.GetData());

	EditorFrameContext& editorFrameContext = static_cast<EditorFrameContext&>(scui::ApplicationUI::GetCurrentContext().GetCurrentFrame());
	editorFrameContext.cloudscapeEditorState = editorFrameContext.GetFrameMemoryArena().AllocateType<CloudscapeEditorFrameState>();
	CloudscapeEditorFrameState& edFrameState = *editorFrameContext.cloudscapeEditorState;

	editorFrameContext.cloudscapeEditorStateDeleter = [](CloudscapeEditorFrameState* state)
	{
		state->~CloudscapeEditorFrameState();
	};

	Super::DrawUI();

	if (m_cloudscapeAsset.IsValid())
	{
		ImGui::Text("Cloudscape Asset: %s", m_cloudscapeAsset->GetName().GetData());
	}
	else
	{
		ImGui::TextUnformatted("Cloudscape Asset: <None>");
	}

	if (m_cloudscapeSaveJob.IsFinished())
	{
		m_cloudscapeSaveJob.Reset();
	}

	const Bool isSavingCloudscapeAsset = m_cloudscapeSaveJob.IsValid();
	if (isSavingCloudscapeAsset)
	{
		ImGui::TextUnformatted("Saving Cloudscape Asset...");
	}

	if (m_paintedWeatherMapLoadJob.IsFinished())
	{
		m_paintedWeatherMap = m_paintedWeatherMapLoadJob.GetResult();
		m_paintedWeatherMapLoadJob.Reset();
		SPT_CHECK(!!m_paintedWeatherMap);
		SPT_CHECK(m_paintedWeatherMap->GetRHI().IsValid());
	}

	const Bool isLoadingWeatherMap = m_paintedWeatherMapLoadJob.IsValid();

	ImGui::BeginDisabled(!m_cloudscapeAsset.IsValid() || isSavingCloudscapeAsset);

	if (m_cloudscapeAsset.IsValid())
	{
		const Bool canEdit = !isSavingCloudscapeAsset && !isLoadingWeatherMap;

		if (canEdit)
		{
			if (ImGui::Button("Save Asset"))
			{
				m_isWeatherMapEditingEnabled = false;

				m_cloudscapeSaveJob = js::Launch(SPT_GENERIC_JOB_NAME, 
												[this, paintedWeatherMap = std::move(m_paintedWeatherMap)]()
												{
													if (paintedWeatherMap)
													{
														const as::CloudscapeAssetDefinition& assetDef = m_cloudscapeAsset->GetCloudscapeAssetDefinition();
														const lib::Path weatherMapPath = m_cloudscapeAsset->GetDirectoryPath() / assetDef.weatherMapTex;
														gfx::TextureWriter::SaveTexture(paintedWeatherMap->GetTexture(), weatherMapPath.generic_string());
													}
									
													m_cloudscapeAsset->SaveAsset();
												});
			}
			if (ImGui::Checkbox("Enable Weather Map Editing", &m_isWeatherMapEditingEnabled))
			{
				if (m_isWeatherMapEditingEnabled && !m_paintedWeatherMap)
				{
					const as::CloudscapeAssetDefinition& assetDef = m_cloudscapeAsset->GetCloudscapeAssetDefinition();
					const lib::Path weatherMapPath = m_cloudscapeAsset->GetDirectoryPath() / assetDef.weatherMapTex;

					m_paintedWeatherMapLoadJob = js::Launch(SPT_GENERIC_JOB_NAME,
															[weatherMapPath]() -> lib::SharedPtr<rdr::TextureView>
															{
																gfx::TextureLoadParams loadParams;
																loadParams.usage       = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::StorageTexture);
																loadParams.memoryUsage = rhi::EMemoryUsage::GPUToCpu;
																loadParams.forceTiling = rhi::ETextureTiling::Linear;
																const lib::SharedPtr<rdr::Texture> texture = gfx::TextureLoader::LoadTexture(weatherMapPath.generic_string(), loadParams);
																//rhi::TextureDefinition textureDef;
																//textureDef.resolution = math::Vector3u(2048u, 2048u, 1u);
																//textureDef.format     = rhi::EFragmentFormat::RGBA16_UN_Float;
																//textureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::StorageTexture);
																//textureDef.tiling	 = rhi::ETextureTiling::Linear;
																//const lib::SharedPtr<rdr::Texture> texture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Painted Weather Map Texture"), textureDef, rhi::EMemoryUsage::GPUToCpu);

																return texture->CreateView(RENDERER_RESOURCE_NAME("Painted Weather Map Texture View"));
															});
				}
			}

			ImGui::Separator();
			ImGui::TextUnformatted("Weather Map Paint Settings");

			if (m_isWeatherMapEditingEnabled && m_paintedWeatherMap)
			{
				ImGui::SliderFloat("Brush Size", &m_brushSize, 100.f, 2000.f);
				ImGui::SliderFloat("Brush Falloff", &m_brushFalloff, 0.f, 1.f);
				ImGui::SliderFloat("Paint Value", &m_paintValue, 0.f, 1.f);

				const char* weatherChannels[] = { "Coverage", "Type", "B", "A" };
				ImGui::Combo("Paint Channel", (int*)&m_weatherChannelToPaint, weatherChannels, EWeatherMapChannel::Count);

				if (m_isWeatherMapEditingEnabled)
				{
					rsc::editor::CloudscapeInfluenceGizmo gizmo;
					gizmo.radius  = m_brushSize;
					gizmo.color   = math::Vector3f(0.f, 1.f, 1.f);
					gizmo.opacity = 0.3f;
					edFrameState.activeCloudscapeGizmo = gizmo;
				}

				if (m_viewportInterface.IsFocused())
				{
					if (inp::InputManager::Get().WasKeyJustPressed(inp::EKey::LeftMouseButton))
					{
						m_paintMode = EWeatherMapPaintMode::Paint;
					}
				}

				if (m_paintMode != EWeatherMapPaintMode::None && !inp::InputManager::Get().IsKeyPressed(inp::EKey::LeftMouseButton))
				{
					m_paintMode = EWeatherMapPaintMode::None;
				}

				if (m_paintMode != EWeatherMapPaintMode::None)
				{
					m_paintMode = inp::InputManager::Get().IsKeyPressed(inp::EKey::LShift) ? EWeatherMapPaintMode::Erase : EWeatherMapPaintMode::Paint;
				}

				if (m_paintMode != EWeatherMapPaintMode::None)
				{
					const Real32 paintValue = (m_paintMode == EWeatherMapPaintMode::Paint) ? m_paintValue : -m_paintValue;

					rsc::editor::WeatherMapPaintCommand paintCommand;
					paintCommand.channelToPaint = static_cast<Uint32>(m_weatherChannelToPaint);
					paintCommand.value          = paintValue * editorFrameContext.GetDeltaTime();
					edFrameState.weatherMapPaintCommand = paintCommand;
				}
			}
			else
			{
				m_paintMode = EWeatherMapPaintMode::None;
			}
		}

		if (!isLoadingWeatherMap && m_paintedWeatherMap)
		{
			SPT_CHECK(m_paintedWeatherMap->GetRHI().IsValid());
			edFrameState.paintedWeatherMap = m_paintedWeatherMap;
		}
	}
	else
	{
		ImGui::TextWrapped("Null Cloudscape Asset");
	}

	ImGui::EndDisabled();

	ImGui::End();
}

} // spt::ed
