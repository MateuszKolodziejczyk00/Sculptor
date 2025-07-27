#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"
#include "Transfers/UploadUtils.h"
#include "ResourcesManager.h"


namespace spt::gfx
{

template<typename TStruct, SizeType size, rhi::EMemoryUsage memoryUsage>
class StructuredCPUToGPUBufferBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit StructuredCPUToGPUBufferBinding(const lib::HashedString& name)
		: Super(name)
		, m_dataSize(sizeof(TStruct) * size)
		, m_dataIsDirty(true)
		, m_buffer(rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(GetName()), rhi::BufferDefinition(m_dataSize * 2, GetBufferUsageFlags()), memoryUsage))
		, m_writeIdx(false)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetIndexer& indexer) const final
	{
		SPT_CHECK(!IsDirty());
		const rdr::BufferView bufferView = CreateBufferViewToBind();
		bufferView.GetBuffer()->GetRHI().CopyUAVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), indexer[GetBaseBindingIdx()][0]);
	}

	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		SPT_CHECK(!IsDirty());
		builder.AddBufferAccess(CreateBufferViewToBind(), rg::ERGBufferAccess::Read);
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return rdr::shader_translator::DefineType<TStruct>() + '\n' +
			BuildBindingVariableCode(lib::String("StructuredBuffer") + '<' + rdr::shader_translator::GetTypeName<TStruct>() + "> " + name, bindingIdx);
	}
	
	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::StorageBuffer, smd::EBindingFlags::Storage) };
	}

	const TStruct& operator[](SizeType idx) const
	{
		SPT_CHECK(idx < size);
		return m_data[idx];
	}

	TStruct& GetMutable(SizeType idx)
	{
		SPT_CHECK(idx < size);
		m_dataIsDirty = true;
		return m_data[idx];
	}

	Bool IsDirty() const
	{
		return m_dataIsDirty;
	}

	void Flush()
	{
		if (m_dataIsDirty)
		{
			UploadDataToBuffer(m_buffer, GetWriteOffset(), reinterpret_cast<const Byte*>(m_data.data()), m_dataSize);

			m_writeIdx = !m_writeIdx;
			m_dataIsDirty = false;

			MarkAsDirty();
		}
	}

	SizeType GetSize() const
	{
		return size;
	}

private:

	Uint64 GetWriteOffset() const
	{
		return m_writeIdx ? m_dataSize : 0;
	}

	Uint64 GetReadOffset() const
	{
		return m_writeIdx ? 0 : m_dataSize;
	}

	rdr::BufferView CreateBufferViewToBind() const
	{
		return rdr::BufferView(m_buffer, GetReadOffset(), m_dataSize);
	}

	static constexpr rhi::EBufferUsage GetBufferUsageFlags()
	{
		rhi::EBufferUsage flags = rhi::EBufferUsage::Storage;
		if constexpr (memoryUsage == rhi::EMemoryUsage::GPUOnly)
		{
			lib::AddFlag(flags, rhi::EBufferUsage::TransferDst);
		}

		return flags;
	}

	lib::StaticArray<TStruct, size> m_data;

	const Uint64 m_dataSize;

	Bool m_dataIsDirty;

	lib::SharedRef<rdr::Buffer> m_buffer;
	Bool m_writeIdx;
};

} // spt::gfx
