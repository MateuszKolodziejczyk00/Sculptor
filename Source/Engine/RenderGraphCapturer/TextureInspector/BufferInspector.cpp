#include "BufferInspector.h"
#include "RenderGraphCaptureTypes.h"
#include "ImGui/SculptorImGui.h"
#include "ShaderStructsRegistry.h"
#include "ImGui/DockBuilder.h"
#include "RenderGraphCaptureViewer.h"
#include "MaterialTypes.h"
#include "MaterialsSubsystem.h"


namespace spt::rg::capture
{

namespace priv
{

constexpr Uint32 elementsPerPage = 100u;

constexpr SizeType rawBytesPerWord = sizeof(Uint32);
constexpr SizeType rawWordsPerRow  = 4u;
constexpr SizeType rawBytesPerRow  = rawBytesPerWord * rawWordsPerRow;
constexpr SizeType rawBytesPerPage = 1024u;

Uint32 GetTypeSize(const lib::HashedString typeName)
{
#if SHADER_STRUCTS_RTTI
	const rdr::ShaderStructMetaData* structMetaData = rdr::ShaderStructsRegistry::GetStructMetaData(typeName.ToString());
	if (structMetaData)
	{
		return structMetaData->GetRTTI().size;
	}
	else if (typeName == lib::HashedString("uint"))
	{
		return sizeof(Uint32);
	}
	else if (typeName == lib::HashedString("uint2"))
	{
		return sizeof(Uint32) * 2;
	}
	else if (typeName == lib::HashedString("uint3"))
	{
		return sizeof(Uint32) * 3;
	}
	else if (typeName == lib::HashedString("uint4"))
	{
		return sizeof(Uint32) * 4;
	}
	else if (typeName == lib::HashedString("int"))
	{
		return sizeof(Int32);
	}
	else if (typeName == lib::HashedString("int2"))
	{
		return sizeof(Int32) * 2;
	}
	else if (typeName == lib::HashedString("int3"))
	{
		return sizeof(Int32) * 3;
	}
	else if (typeName == lib::HashedString("int4"))
	{
		return sizeof(Int32) * 4;
	}
	else if (typeName == lib::HashedString("float"))
	{
		return sizeof(Real32);
	}
	else if (typeName == lib::HashedString("float2"))
	{
		return sizeof(Real32) * 2;
	}
	else if (typeName == lib::HashedString("float3"))
	{
		return sizeof(Real32) * 3;
	}
	else if (typeName == lib::HashedString("float4"))
	{
		return sizeof(Real32) * 4;
	}
	else if (typeName == lib::HashedString("float4x4"))
	{
		return sizeof(Real32) * 16;
	}
	else if (typeName == lib::HashedString("bool"))
	{
		return sizeof(Uint32);
	}
	else if (typeName == lib::HashedString("uint16_t"))
	{
		return sizeof(Uint16);
	}
	else if (typeName == lib::HashedString("NamedBufferDescriptorIdx"))
	{
		return sizeof(Uint32);
	}
	else if (typeName.ToString().starts_with("UAVTexture") || typeName.ToString().starts_with("SRVTexture"))
	{
		return 2 * sizeof(Uint32);
	}
	else
	{
		return 0u;
	}
#endif // SHADER_STRUCTS_RTTI

	return 0u;
}

Int32 AlignToRawRow(Uint64 offset)
{
	return static_cast<Int32>(offset - offset % rawBytesPerRow);
}

lib::String FormatRawWord(const Byte* word, ERawDataFormat format)
{
	switch (format)
	{
	case ERawDataFormat::Hex:   return std::format("{:08X}", *reinterpret_cast<const Uint32*>(word));
	case ERawDataFormat::Uint:  return std::format("{}", *reinterpret_cast<const Uint32*>(word));
	case ERawDataFormat::Int:   return std::format("{}", *reinterpret_cast<const Int32*>(word));
	case ERawDataFormat::Float: return std::format("{}", *reinterpret_cast<const Real32*>(word));
	}

	SPT_CHECK_NO_ENTRY();
	return {};
}

template<typename TScalar>
void DrawScalars(lib::Span<const Byte> data, Uint32 scalarsNum)
{
	const TScalar* values = reinterpret_cast<const TScalar*>(data.data());

	lib::String text = std::format("{}", values[0]);
	for (Uint32 idx = 1u; idx < scalarsNum; ++idx)
	{
		text += std::format(", {}", values[idx]);
	}

	ImGui::TextUnformatted(scalarsNum > 1u ? std::format("[ {} ]", text).c_str() : text.c_str());
}

template<typename TScalar>
Bool TryDrawVector(lib::StringView typeName, lib::StringView scalarName, lib::Span<const Byte> data)
{
	if (!typeName.starts_with(scalarName))
	{
		return false;
	}

	const lib::StringView dimension = typeName.substr(scalarName.size());
	if (dimension.empty())
	{
		DrawScalars<TScalar>(data, 1u);
		return true;
	}

	if (dimension.size() == 1u && dimension[0] >= '2' && dimension[0] <= '4')
	{
		DrawScalars<TScalar>(data, static_cast<Uint32>(dimension[0] - '0'));
		return true;
	}

	return false;
}

Bool BeginBoundDataRow(lib::StringView name, ImGuiTreeNodeFlags flags)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	return ImGui::TreeNodeEx("##Row", flags | ImGuiTreeNodeFlags_SpanFullWidth, "%.*s", static_cast<Int32>(name.size()), name.data());
}

void DrawRowTypeAndOffset(const char* typeName, Uint32 offset)
{
	ImGui::TableSetColumnIndex(2);
	ImGui::TextDisabled("%s", typeName);
	ImGui::TableNextColumn();
	ImGui::TextDisabled("0x%X", offset);
}

} // priv

BufferInspector::BufferInspector(const scui::ViewDefinition& definition, RGNodeCaptureViewer& parentNodeViewer, const BufferInspectParams& inspectParams)
	: Super(definition)
	, m_inspectParams(inspectParams)
	, m_parentNodeViewer(parentNodeViewer)
	, m_boundDataPanelName(CreateUniqueName("Bound Data"))
	, m_bufferDataPanelName(CreateUniqueName("Buffer Data"))
{
	m_byteViewerOffset = priv::AlignToRawRow(inspectParams.offset);
}

void BufferInspector::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID, ui::Split(ui::ESplit::Horizontal, 0.6f,
									 ui::DockWindow(m_boundDataPanelName),
									 ui::DockWindow(m_bufferDataPanelName)));
}

void BufferInspector::DrawUI()
{
#if SHADER_STRUCTS_RTTI
	SPT_PROFILER_FUNCTION();

	Super::DrawUI();

	const Bool isDownloading = !!m_inspectParams.bufferVersion->downloadedBuffer;
	const lib::Span<const Byte> bufferData = m_inspectParams.bufferVersion->bufferData;

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_boundDataPanelName.GetData());

	if (isDownloading)
	{
		ImGui::TextDisabled("Downloading buffer...");
	}
	else
	{
		DrawBoundData(bufferData);
	}

	ImGui::End();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_bufferDataPanelName.GetData());

	if (isDownloading)
	{
		ImGui::TextDisabled("Downloading buffer...");
	}
	else
	{
		DrawRawData(bufferData);
	}

	ImGui::End();
#endif // SHADER_STRUCTS_RTTI
}

void BufferInspector::DrawBoundData(lib::Span<const Byte> bufferData)
{
	if (bufferData.empty() || !m_inspectParams.structTypeName.IsValid())
	{
		ImGui::TextDisabled("No typed data was captured for this binding");
		return;
	}

	const Uint32 typeSize = priv::GetTypeSize(m_inspectParams.structTypeName);
	if (typeSize == 0u)
	{
		ImGui::TextDisabled("No size data for type: %s", m_inspectParams.structTypeName.GetData());
		return;
	}

	const auto& elementsRange = m_inspectParams.elementsRange;

	const Uint64 viewSize    = m_inspectParams.size > 0u ? m_inspectParams.size : bufferData.size() - m_inspectParams.offset;
	const Uint32 elementsNum = elementsRange ? elementsRange->second - elementsRange->first : static_cast<Uint32>(viewSize / typeSize);

	ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", m_inspectParams.bufferVersion->owningBuffer->name.GetData());
	ImGui::SameLine();
	ImGui::TextDisabled("%s, %u elements, version %u", m_inspectParams.structTypeName.GetData(), elementsNum, m_inspectParams.bufferVersion->versionIdx);

	if (!elementsRange && elementsNum > priv::elementsPerPage)
	{
		ImGui::SliderInt("First Element", &m_dataViewerBeginidx, 0, static_cast<Int32>(elementsNum - priv::elementsPerPage));
	}

	const Uint32 beginIdx = elementsRange ? elementsRange->first : static_cast<Uint32>(m_dataViewerBeginidx);
	const Uint32 endIdx   = elementsRange ? elementsRange->second : std::min(beginIdx + priv::elementsPerPage, elementsNum);

	const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoSavedSettings;
	if (ImGui::BeginTable("##BoundData", 4, tableFlags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.35f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.4f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.25f);
		ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		for (Uint32 idx = beginIdx; idx < endIdx; ++idx)
		{
			if (endIdx - beginIdx > 1u)
			{
				ImGui::SetNextItemOpen(false, ImGuiCond_Once);
			}

			const Uint64 elemOffset = m_inspectParams.offset + idx * typeSize;
			DrawStruct(std::format("[{}]", idx), m_inspectParams.structTypeName, bufferData.subspan(elemOffset, typeSize), static_cast<Uint32>(elemOffset));
		}

		ImGui::EndTable();
	}
}

void BufferInspector::DrawRawData(lib::Span<const Byte> bufferData)
{
	const char* formats[] = { "Hex", "Uint", "Int", "Float" };

	ImGui::SetNextItemWidth(ImGui::CalcTextSize("Float").x + ImGui::GetFrameHeight() * 2.f);
	ImGui::Combo("Format", reinterpret_cast<int*>(&m_rawDataFormat), formats, SPT_ARRAY_SIZE(formats));

	const Uint64 boundBegin = m_inspectParams.offset;
	const Uint64 boundEnd   = boundBegin + (m_inspectParams.size > 0u ? m_inspectParams.size : bufferData.size() - boundBegin);

	ImGui::SameLine();
	if (ImGui::Button("Go To Bound Data"))
	{
		m_byteViewerOffset = priv::AlignToRawRow(boundBegin);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Bound range (highlighted): 0x%llX - 0x%llX", boundBegin, boundEnd);
	}

	if (ImGui::SliderInt("Byte Offset", &m_byteViewerOffset, 0, std::max(static_cast<Int32>(bufferData.size()) - static_cast<Int32>(priv::rawBytesPerPage), 0)))
	{
		m_byteViewerOffset = priv::AlignToRawRow(m_byteViewerOffset);
	}

	const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoSavedSettings;
	if (ImGui::BeginTable("##RawData", 1 + static_cast<Int32>(priv::rawWordsPerRow), tableFlags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed);
		for (const char* wordColumn : { "+0", "+4", "+8", "+C" })
		{
			ImGui::TableSetupColumn(wordColumn);
		}
		ImGui::TableHeadersRow();

		const SizeType end = std::min<SizeType>(bufferData.size(), m_byteViewerOffset + priv::rawBytesPerPage);
		for (SizeType rowOffset = m_byteViewerOffset; rowOffset < end; rowOffset += priv::rawBytesPerRow)
		{
			ImGui::TableNextRow();

			if (rowOffset < boundEnd && rowOffset + priv::rawBytesPerRow > boundBegin)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImGuiCol_Header));
			}

			ImGui::TableNextColumn();
			ImGui::TextDisabled("%08zX", rowOffset);

			for (SizeType wordIdx = 0u; wordIdx < priv::rawWordsPerRow; ++wordIdx)
			{
				ImGui::TableNextColumn();

				const SizeType wordOffset = rowOffset + wordIdx * priv::rawBytesPerWord;
				if (wordOffset + priv::rawBytesPerWord <= bufferData.size())
				{
					const Byte* word = &bufferData[wordOffset];
					ImGui::TextUnformatted(priv::FormatRawWord(word, m_rawDataFormat).c_str());
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Offset: 0x%zX\nHex: %s\nUint: %s\nInt: %s\nFloat: %s",
										  wordOffset,
										  priv::FormatRawWord(word, ERawDataFormat::Hex).c_str(),
										  priv::FormatRawWord(word, ERawDataFormat::Uint).c_str(),
										  priv::FormatRawWord(word, ERawDataFormat::Int).c_str(),
										  priv::FormatRawWord(word, ERawDataFormat::Float).c_str());
					}
				}
			}
		}

		ImGui::EndTable();
	}
}

void BufferInspector::DrawStruct(lib::StringView name, lib::HashedString typeName, lib::Span<const Byte> data, Uint32 currentOffset)
{
#if SHADER_STRUCTS_RTTI
	ImGui::PushID(currentOffset);

	const lib::String typeNameStr = typeName.ToString();

	const rdr::ShaderStructMetaData* structMetaData = typeName == lib::HashedString("MaterialDataHandle") ? nullptr : rdr::ShaderStructsRegistry::GetStructMetaData(typeNameStr);
	if (structMetaData)
	{
		const Bool isOpen = priv::BeginBoundDataRow(name, ImGuiTreeNodeFlags_DefaultOpen);
		priv::DrawRowTypeAndOffset(typeNameStr.c_str(), currentOffset);

		if (isOpen)
		{
			for (const rdr::ShaderStructMemberRTTI& member : structMetaData->GetRTTI().members)
			{
				if (member.elementsNum > 1u)
				{
					ImGui::PushID(member.offset);

					const Bool isArrayOpen = priv::BeginBoundDataRow(std::format("{} [{}]", member.memberName, member.elementsNum), ImGuiTreeNodeFlags_None);
					priv::DrawRowTypeAndOffset(member.memberTypeName.c_str(), currentOffset + member.offset);

					if (isArrayOpen)
					{
						for (Uint32 i = 0u; i < member.elementsNum; ++i)
						{
							const Uint32 elemDataOffset = member.offset + i * member.stride;
							DrawStruct(std::format("[{}]", i), member.memberTypeName, data.subspan(elemDataOffset, member.stride), currentOffset + elemDataOffset);
						}
						ImGui::TreePop();
					}

					ImGui::PopID();
				}
				else
				{
					DrawStruct(member.memberName, member.memberTypeName, data.subspan(member.offset), currentOffset + member.offset);
				}
			}
			ImGui::TreePop();
		}
	}
	else
	{
		priv::BeginBoundDataRow(name, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
		ImGui::TableNextColumn();
		DrawValue(typeNameStr, data);
		priv::DrawRowTypeAndOffset(typeNameStr.c_str(), currentOffset);
	}

	ImGui::PopID();
#endif // SHADER_STRUCTS_RTTI
}

void BufferInspector::DrawValue(const lib::String& typeName, lib::Span<const Byte> data)
{
	if (priv::TryDrawVector<Uint32>(typeName, "uint", data)
		|| priv::TryDrawVector<Int32>(typeName, "int", data)
		|| priv::TryDrawVector<Real32>(typeName, "float", data))
	{
		return;
	}

	const RGCapture& capture = m_parentNodeViewer.GetCapture();

	if (typeName == "MaterialDataHandle")
	{
		if (ImGui::BeginPopup("Material Data Selection"))
		{
			ImGui::Text("Select Material Data:");

			const lib::DynamicArray<lib::HashedString> materialDataStructNames = mat::MaterialsSubsystem::Get().GetMaterialDataStructNames();

			for (const lib::HashedString& materialDataStructName : materialDataStructNames)
			{
				if (ImGui::Button(materialDataStructName.GetData()))
				{
					const CapturedPass& pass = m_parentNodeViewer.GetCapturedPass();
					for (const CapturedBufferBinding& bufferBinding : pass.buffers)
					{
						if (bufferBinding.bufferVersion->owningBuffer->name == lib::HashedString("MaterialsUnifiedData"))
						{
							BufferInspectParams inspectParams;
							inspectParams.bufferVersion  = bufferBinding.bufferVersion;
							inspectParams.structTypeName = materialDataStructName;
							inspectParams.offset         = *reinterpret_cast<const Uint16*>(data.data()) * mat::constants::materialDataAlignment;
							inspectParams.elementsRange  = std::make_pair(0u, 1u);
							m_parentNodeViewer.OpenBufferCapture(inspectParams);

							break;
						}
					}
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}

		if (ImGui::SmallButton("Open"))
		{
			const CapturedPass& pass = m_parentNodeViewer.GetCapturedPass();

			for (const CapturedBufferBinding& bufferBinding : pass.buffers)
			{
				if (bufferBinding.bufferVersion->owningBuffer->name == lib::HashedString("MaterialsUnifiedData"))
				{
					ImGui::OpenPopup("Material Data Selection");
				}
			}
		}

		ImGui::SameLine();

		ImGui::Text("Handle ID: %u", *reinterpret_cast<const Uint16*>(data.data()));
	}
	else if (typeName == "float4x4")
	{
		for (Uint32 rowIdx = 0u; rowIdx < 4u; ++rowIdx)
		{
			priv::DrawScalars<Real32>(data.subspan(rowIdx * 4u * sizeof(Real32)), 4u);
		}
	}
	else if (typeName == "uint16_t")
	{
		priv::DrawScalars<Uint16>(data, 1u);
	}
	else if (typeName == "bool")
	{
		ImGui::TextUnformatted(*reinterpret_cast<const Uint32*>(data.data()) > 0u ? "True" : "False");
	}
	else if (typeName == "NamedBufferDescriptorIdx")
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		const Uint32 descriptorIdx = typedData[0];

		if (descriptorIdx != idxNone<Uint32>)
		{
			const auto bufferIt = capture.descriptorIdxToBuffer.find(descriptorIdx);
			const CapturedBuffer* capturedBuffer = bufferIt != capture.descriptorIdxToBuffer.cend() ? bufferIt->second : nullptr;
			if (capturedBuffer)
			{
				if (ImGui::SmallButton(capturedBuffer->name.GetData()))
				{
					// TODO
				}
			}
			else
			{
				ImGui::TextDisabled("Unknown buffer");
			}
		}
		else
		{
			ImGui::TextDisabled("Null Descriptor");
		}
	}
	else if (typeName.starts_with("UAVTexture") || typeName.starts_with("SRVTexture"))
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		const Uint32 descriptorIdx = typedData[0];

		if (descriptorIdx != idxNone<Uint32>)
		{
			const auto textureIt = capture.descriptorIdxToTexture.find(descriptorIdx);
			const CapturedTexture* capturedTexture = textureIt != capture.descriptorIdxToTexture.cend() ? textureIt->second : nullptr;
			if (capturedTexture)
			{
				if (ImGui::SmallButton(capturedTexture->name.GetData()))
				{
					const CapturedPass& pass = m_parentNodeViewer.GetCapturedPass();
					// Find binding for this texture in current pass
					Bool found = false;
					for (const CapturedTextureBinding& binding : pass.textures)
					{
						if (binding.textureVersion->owningTexture == capturedTexture)
						{
							m_parentNodeViewer.OpenTextureCapture(binding);
							found = true;
							break;
						}
					}

					if (!found)
					{
						TextureInspectParams inspectParams;
						inspectParams.texture       = capturedTexture;
						inspectParams.mipIdx        = 0u;
						inspectParams.arrayLayerIdx = 0u;
						inspectParams.versionIdx    = 0u;
						m_parentNodeViewer.OpenTextureCapture(inspectParams);
					}
				}
			}
			else
			{
				ImGui::TextDisabled("Unknown texture");
			}
		}
		else
		{
			ImGui::TextDisabled("Null Descriptor");
		}
	}
	else if (typeName.starts_with("GPUNamedElemPtr"))
	{
		SizeType namedBufferBegin = typeName.find('_');
		SPT_CHECK(namedBufferBegin != lib::String::npos);
		namedBufferBegin += 1u;

		const SizeType namedBufferEnd = typeName.find(',');
		SPT_CHECK(namedBufferEnd != lib::String::npos);
		SPT_CHECK(namedBufferEnd > namedBufferBegin);

		const lib::HashedString namedBuffer = typeName.substr(namedBufferBegin, namedBufferEnd - namedBufferBegin);

		const CapturedPass& pass = m_parentNodeViewer.GetCapturedPass();
		const auto foundNamedBuffer = pass.namedBuffersCaptures.find(namedBuffer);
		if (foundNamedBuffer != pass.namedBuffersCaptures.cend())
		{
			const CapturedBufferBinding& buffer = pass.buffers[foundNamedBuffer->second];

			if (ImGui::SmallButton("Open"))
			{
				const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());

				BufferInspectParams inspectParams;
				inspectParams.bufferVersion  = buffer.bufferVersion;
				inspectParams.elementsRange  = std::make_pair(typedData[0], typedData[0] + 1u);
				inspectParams.structTypeName = buffer.structTypeName;
				m_parentNodeViewer.OpenBufferCapture(inspectParams);
			}

			ImGui::SameLine();
		}

		ImGui::Text("Element Idx: %u", *reinterpret_cast<const Uint32*>(data.data()));
	}
	else if (typeName.starts_with("GPUNamedElemsSpan"))
	{
		SizeType namedBufferBegin = typeName.find('_');
		SPT_CHECK(namedBufferBegin != lib::String::npos);
		namedBufferBegin += 1u;

		const SizeType namedBufferEnd = typeName.find(',');
		SPT_CHECK(namedBufferEnd != lib::String::npos);
		SPT_CHECK(namedBufferEnd > namedBufferBegin);

		const lib::HashedString namedBuffer = typeName.substr(namedBufferBegin, namedBufferEnd - namedBufferBegin);

		const CapturedPass& pass = m_parentNodeViewer.GetCapturedPass();
		const auto foundNamedBuffer = pass.namedBuffersCaptures.find(namedBuffer);
		if (foundNamedBuffer != pass.namedBuffersCaptures.cend())
		{
			const CapturedBufferBinding& buffer = pass.buffers[foundNamedBuffer->second];

			if (ImGui::SmallButton("Open"))
			{
				const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());

				BufferInspectParams inspectParams;
				inspectParams.bufferVersion  = buffer.bufferVersion;
				inspectParams.elementsRange  = std::make_pair(typedData[0], typedData[0] + typedData[1]);
				inspectParams.structTypeName = buffer.structTypeName;
				m_parentNodeViewer.OpenBufferCapture(inspectParams);
			}

			ImGui::SameLine();
		}

		ImGui::Text("Begin Idx: %u, Size: %u", *reinterpret_cast<const Uint32*>(data.data()), *reinterpret_cast<const Uint32*>(data.data() + sizeof(Uint32)));
	}
	else if (typeName.starts_with("NamedBufferDescriptor") || typeName.starts_with("TypedBuffer"))
	{
		if (ImGui::SmallButton("Open"))
		{
			SizeType typeBegin = typeName.find('<');
			SPT_CHECK(typeBegin != lib::String::npos);
			typeBegin += 1u;

			const SizeType typeEnd = typeName.find('>');
			SPT_CHECK(typeEnd != lib::String::npos);
			SPT_CHECK(typeEnd > typeBegin);

			const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
			const Uint32 descriptorIdx = typedData[0];

			const auto foundBuffer = capture.descriptorIdxToBuffer.find(descriptorIdx);
			if (foundBuffer != capture.descriptorIdxToBuffer.cend())
			{
				const CapturedBuffer& capturedBuffer = *foundBuffer->second;

				lib::HashedString structTypeName = typeName.substr(typeBegin, typeEnd - typeBegin);

				BufferInspectParams inspectParams;
				inspectParams.bufferVersion  = capturedBuffer.versions.front().get();
				inspectParams.structTypeName = structTypeName;
				m_parentNodeViewer.OpenBufferCapture(inspectParams);
			}
		}
	}
	else
	{
		ImGui::TextDisabled("Unsupported type");
	}
}

} // spt::rg::capture
