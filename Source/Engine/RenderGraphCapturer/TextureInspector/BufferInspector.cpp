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

} // priv

BufferInspector::BufferInspector(const scui::ViewDefinition& definition, RGNodeCaptureViewer& parentNodeViewer, const BufferInspectParams& inspectParams)
	: Super(definition)
	, m_inspectParams(inspectParams)
	, m_parentNodeViewer(parentNodeViewer)
	, m_boundDataPanelName(CreateUniqueName("Bound Data"))
	, m_bufferDataPanelName(CreateUniqueName("Buffer Data"))
{
}

void BufferInspector::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID, ui::Split(ui::ESplit::Horizontal, 0.7f,
									 ui::DockWindow(m_boundDataPanelName),
									 ui::DockWindow(m_bufferDataPanelName)));
}

void BufferInspector::DrawUI()
{
#if SHADER_STRUCTS_RTTI
	SPT_PROFILER_FUNCTION();

	Super::DrawUI();
	
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());

	ImGui::Begin(m_boundDataPanelName.GetData());

	if (m_inspectParams.bufferVersion->downloadedBuffer)
	{
		ImGui::Text("Downloading buffer...");
	}
	else
	{
		const lib::Span<const Byte> bufferData = m_inspectParams.bufferVersion->bufferData;

		if (m_inspectParams.structTypeName.IsValid() && !bufferData.empty())
		{
			ImGui::Columns(3);

			const Uint32 typeSize = priv::GetTypeSize(m_inspectParams.structTypeName);
			if(typeSize != 0u)
			{
				Uint32 beginIdx = 0u;
				Uint32 endIdx   = 0u;

				if (m_inspectParams.elementsRange.has_value())
				{
					beginIdx = m_inspectParams.elementsRange->first;
					endIdx   = m_inspectParams.elementsRange->second;
				}
				else
				{
					const Uint64 viewSize = m_inspectParams.size > 0u ? m_inspectParams.size : m_inspectParams.bufferVersion->bufferData.size() - m_inspectParams.offset;
					const Uint32 elementsNum = static_cast<Uint32>(viewSize / typeSize);

					ImGui::SliderInt("Begin Idx", &m_dataViewerBeginidx, 0, std::max<Int32>(elementsNum - 100, 0));

					beginIdx = static_cast<Uint32>(m_dataViewerBeginidx);
					endIdx = std::min(beginIdx + 100u, elementsNum);
				}

				for (Uint32 idx = beginIdx; idx < endIdx; ++idx)
				{
					const Uint32 elemOffset = idx * typeSize;
					const lib::String elemName = lib::String("[") + std::to_string(idx) + "]";
					DrawStruct(elemName, m_inspectParams.structTypeName, bufferData.subspan(m_inspectParams.offset + elemOffset, typeSize), elemOffset);
				}
			}
			else
			{
				ImGui::Text("No size data for type: %s", m_inspectParams.structTypeName.GetData());
			}

			ImGui::EndColumns();
			ImGui::Separator();
		}

		ImGui::End();

		ImGui::Begin(m_bufferDataPanelName.GetData());

		ImGui::SliderInt("Byte Offset", &m_byteViewerOffset, 0, std::max(static_cast<Int32>(bufferData.size()) - 1024, 0));

		ImGui::Columns(11);
		// Display data in rows of 16 bytes
		const SizeType end = std::min<SizeType>(bufferData.size(), m_byteViewerOffset + 1024); // limit to first 1KB for performance
		for (SizeType i = m_byteViewerOffset; i < end; i += 16)
		{
			ImGui::Text("%d: ", static_cast<Uint32>(i));
			ImGui::NextColumn();

			for (SizeType j = 0; j < 4; ++j)
			{
				if (i + j < bufferData.size())
				{
					ImGui::Text("%u ", *reinterpret_cast<const Uint32*>(&bufferData[i + j * sizeof(Uint32)]));
					ImGui::NextColumn();
				}
				else
				{
					ImGui::Text("   ");
					ImGui::NextColumn();
				}
			}
			ImGui::NextColumn();
			for (SizeType j = 0; j < 4; ++j)
			{
				if (i + j < bufferData.size())
				{
					ImGui::Text("%f ", *reinterpret_cast<const Real32*>(&bufferData[i + j * sizeof(Real32)]));
					ImGui::NextColumn();
				}
				else
				{
					ImGui::Text(" ");
					ImGui::NextColumn();
				}
			}

			if (m_inspectParams.offset >= i && m_inspectParams.offset < i + 16)
			{
				ImGui::SameLine();
				ImGui::Text("<- Bound Data");
			}
			ImGui::NextColumn();
		}
	}

	ImGui::End();

#endif // SHADER_STRUCTS_RTTI
}

void BufferInspector::DrawStruct(lib::StringView name, lib::HashedString typeName, lib::Span<const Byte> data, Uint32 currentOffset)
{
#if SHADER_STRUCTS_RTTI
	const RGCapture& capture = m_parentNodeViewer.GetCapture();

	ImGui::PushID(currentOffset);

	ImGui::EndColumns();
	ImGui::Separator();
	ImGui::Columns(3);

	ImGui::Text(name.data());

	ImGui::NextColumn();

	ImGui::Text("offset: %d\t\t%s", currentOffset, typeName.GetView().data());

	ImGui::NextColumn();

	const lib::String typeNameStr = typeName.ToString();

	const rdr::ShaderStructMetaData* memberStructMetaData = rdr::ShaderStructsRegistry::GetStructMetaData(typeNameStr);
	if (typeName == lib::HashedString("MaterialDataHandle"))
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

		if(ImGui::Button(name.data()))
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
	}
	else if (memberStructMetaData)
	{
		const rdr::ShaderStructRTTI& rtti = memberStructMetaData->GetRTTI();
		ImGui::Indent();
		ImGui::PushID(currentOffset);
		for (const rdr::ShaderStructMemberRTTI& member : rtti.members)
		{
			ImGui::NextColumn();

			const Bool isArray = member.elementsNum > 1;
			if (isArray)
			{
				for(Uint32 i = 0; i < member.elementsNum; ++i)
				{
					const lib::String elemName = lib::String("[") + std::to_string(i) + "]";
					const Uint32 elemOffset = currentOffset + member.offset + i * member.stride;
					DrawStruct(elemName.c_str(), member.memberTypeName, data.subspan(member.offset + i * member.stride, member.stride), elemOffset);
				}
			}
			else
			{
				DrawStruct(member.memberName, member.memberTypeName, data.subspan(member.offset), currentOffset + member.offset);
			}
		}
		ImGui::Unindent();
		ImGui::PopID();
	}
	else if (typeName == lib::HashedString("uint"))
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		ImGui::Text("%u", typedData[0]);
	}
	else if (typeName == lib::HashedString("uint2"))
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		ImGui::Text("[ %u, %u ]", typedData[0], typedData[1]);
	}
	else if (typeName == lib::HashedString("uint3"))
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		ImGui::Text("[ %u, %u, %u ]", typedData[0], typedData[1], typedData[2]);
	}
	else if (typeName == lib::HashedString("uint4"))
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		ImGui::Text("[ %u, %u, %u, %u ]", typedData[0], typedData[1], typedData[2], typedData[3]);
	}
	else if (typeName == lib::HashedString("int"))
	{
		const Int32* typedData = reinterpret_cast<const Int32*>(data.data());
		ImGui::Text("%d", typedData[0]);
	}
	else if (typeName == lib::HashedString("int2"))
	{
		const Int32* typedData = reinterpret_cast<const Int32*>(data.data());
		ImGui::Text("[ %d, %d ]", typedData[0], typedData[0 + 1]);
	}
	else if (typeName == lib::HashedString("int3"))
	{
		const Int32* typedData = reinterpret_cast<const Int32*>(data.data());
		ImGui::Text("[ %d, %d, %d ]", typedData[0], typedData[1], typedData[2]);
	}
	else if (typeName == lib::HashedString("int4"))
	{
		const Int32* typedData = reinterpret_cast<const Int32*>(data.data());
		ImGui::Text("[ %d, %d, %d, %d ]", typedData[0], typedData[1], typedData[2], typedData[3]);
	}
	else if (typeName == lib::HashedString("float"))
	{
		const Real32* typedData = reinterpret_cast<const Real32*>(data.data());
		ImGui::Text("%f", typedData[0]);
	}
	else if (typeName == lib::HashedString("float2"))
	{
		const Real32* typedData = reinterpret_cast<const Real32*>(data.data());
		ImGui::Text("[ %f, %f ]", typedData[0], typedData[0 + 1]);
	}
	else if (typeName == lib::HashedString("float3"))
	{
		const Real32* typedData = reinterpret_cast<const Real32*>(data.data());
		ImGui::Text("[ %f, %f, %f ]", typedData[0], typedData[1], typedData[2]);
	}
	else if (typeName == lib::HashedString("float4"))
	{
		const Real32* typedData = reinterpret_cast<const Real32*>(data.data());
		ImGui::Text("[ %f, %f, %f, %f ]", typedData[0], typedData[1], typedData[2], typedData[3]);
	}
	else if (typeName == lib::HashedString("float4x4"))
	{
		const Real32* typedData = reinterpret_cast<const Real32*>(data.data());
		ImGui::Text("%f, %f, %f, %f", typedData[0], typedData[0 * 16 + 1], typedData[0 * 16 + 2], typedData[0 * 16 + 3]);
		ImGui::Text("%f, %f, %f, %f", typedData[4], typedData[0 * 16 + 5], typedData[0 * 16 + 6], typedData[0 * 16 + 7]);
		ImGui::Text("%f, %f, %f, %f", typedData[8], typedData[0 * 16 + 9], typedData[0 * 16 + 10], typedData[0 * 16 + 11]);
		ImGui::Text("%f, %f, %f, %f", typedData[12], typedData[0 * 16 + 13], typedData[0 * 16 + 14], typedData[0 * 16 + 15]);
	}
	else if (typeName == lib::HashedString("uint16_t"))
	{
		const Uint16* typedData = reinterpret_cast<const Uint16*>(data.data());
		ImGui::Text("[ %u ]", Uint32(typedData[0]));
	}
	else if (typeName == lib::HashedString("bool"))
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		ImGui::Text(typedData[0] > 0 ? "True" : "False");
	}
	else if (typeName == lib::HashedString("NamedBufferDescriptorIdx"))
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		const Uint32 descriptorIdx = typedData[0];

		if (descriptorIdx != idxNone<Uint32>)
		{
		const auto bufferIt = capture.descriptorIdxToBuffer.find(descriptorIdx);
		const CapturedBuffer* capturedBuffer = bufferIt != capture.descriptorIdxToBuffer.cend() ? bufferIt->second : nullptr;
		if (capturedBuffer)
		{
			if (ImGui::Button(capturedBuffer->name.GetData()))
			{
				// TODO
			}
		}
		else
		{
			ImGui::Text("Unknown buffer");
		}
		}
		else
		{
			ImGui::Text("Null Descriptor");
		}
	}
	else if (typeNameStr.starts_with("UAVTexture") || typeNameStr.starts_with("SRVTexture"))
	{
		const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
		const Uint32 descriptorIdx = typedData[0];

		if (descriptorIdx != idxNone<Uint32>)
		{
			const auto textureIt = capture.descriptorIdxToTexture.find(descriptorIdx);
			const CapturedTexture* capturedTexture = textureIt != capture.descriptorIdxToTexture.cend() ? textureIt->second : nullptr;
			if (capturedTexture)
			{
				if (ImGui::Button(capturedTexture->name.GetData()))
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
				ImGui::Text("Unknown texture");
			}
		}
		else
		{
			ImGui::Text("Null Descriptor");
		}
	}
	else if (typeNameStr.starts_with("GPUNamedElemPtr"))
	{
		SizeType namedBufferBegin = typeNameStr.find('_');
		SPT_CHECK(namedBufferBegin != lib::String::npos);
		namedBufferBegin += 1u;

		const SizeType namedBufferEnd = typeNameStr.find(',');
		SPT_CHECK(namedBufferEnd != lib::String::npos);
		SPT_CHECK(namedBufferEnd > namedBufferBegin);

		const lib::HashedString namedBuffer = typeNameStr.substr(namedBufferBegin, namedBufferEnd - namedBufferBegin);

		const CapturedPass& pass = m_parentNodeViewer.GetCapturedPass();
		const auto foundNamedBuffer = pass.namedBuffersCaptures.find(namedBuffer);
		if (foundNamedBuffer != pass.namedBuffersCaptures.cend())
		{
			const CapturedBufferBinding& buffer = pass.buffers[foundNamedBuffer->second];

			if (ImGui::Button(name.data()))
			{
				const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());

				BufferInspectParams inspectParams;
				inspectParams.bufferVersion  = buffer.bufferVersion;
				inspectParams.elementsRange  = std::make_pair(typedData[0], typedData[0] + 1u);
				inspectParams.structTypeName = buffer.structTypeName;
				m_parentNodeViewer.OpenBufferCapture(inspectParams);
			}
		}
	}
	else if (typeNameStr.starts_with("GPUNamedElemsSpan"))
	{
		SizeType namedBufferBegin = typeNameStr.find('_');
		SPT_CHECK(namedBufferBegin != lib::String::npos);
		namedBufferBegin += 1u;

		const SizeType namedBufferEnd = typeNameStr.find(',');
		SPT_CHECK(namedBufferEnd != lib::String::npos);
		SPT_CHECK(namedBufferEnd > namedBufferBegin);

		const lib::HashedString namedBuffer = typeNameStr.substr(namedBufferBegin, namedBufferEnd - namedBufferBegin);

		const CapturedPass& pass = m_parentNodeViewer.GetCapturedPass();
		const auto foundNamedBuffer = pass.namedBuffersCaptures.find(namedBuffer);
		if (foundNamedBuffer != pass.namedBuffersCaptures.cend())
		{
			const CapturedBufferBinding& buffer = pass.buffers[foundNamedBuffer->second];

			if (ImGui::Button(name.data()))
			{
				const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());

				BufferInspectParams inspectParams;
				inspectParams.bufferVersion  = buffer.bufferVersion;
				inspectParams.elementsRange  = std::make_pair(typedData[0], typedData[0] + typedData[1]);
				inspectParams.structTypeName = buffer.structTypeName;
				m_parentNodeViewer.OpenBufferCapture(inspectParams);
			}
		}
	}
	else if (typeNameStr.starts_with("NamedBufferDescriptor") || typeNameStr.starts_with("TypedBuffer"))
	{
		if (ImGui::Button(name.data()))
		{
			SizeType typeBegin = typeNameStr.find('<');
			SPT_CHECK(typeBegin != lib::String::npos);
			typeBegin += 1u;

			const SizeType typeEnd = typeNameStr.find('>');
			SPT_CHECK(typeEnd != lib::String::npos);
			SPT_CHECK(typeEnd > typeBegin);

			const Uint32* typedData = reinterpret_cast<const Uint32*>(data.data());
			const Uint32 descriptorIdx = typedData[0];

			const auto foundBuffer = capture.descriptorIdxToBuffer.find(descriptorIdx);
			if (foundBuffer != capture.descriptorIdxToBuffer.cend())
			{
				const CapturedBuffer& capturedBuffer = *foundBuffer->second;

				lib::HashedString structTypeName = typeNameStr.substr(typeBegin, typeEnd - typeBegin);

				BufferInspectParams inspectParams;
				inspectParams.bufferVersion  = capturedBuffer.versions.front().get();
				inspectParams.structTypeName = structTypeName;
				m_parentNodeViewer.OpenBufferCapture(inspectParams);
			}
		}
	}
	else
	{
		ImGui::Text("Unsupported type");
	}

	ImGui::PopID();
#endif // SHADER_STRUCTS_RTTI
}

} // spt::rg::capture
