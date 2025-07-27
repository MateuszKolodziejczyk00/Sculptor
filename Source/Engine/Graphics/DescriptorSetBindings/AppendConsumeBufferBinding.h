#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"


namespace spt::gfx
{

namespace priv
{

template<typename TStorageType, Bool isAppend>
class AppendConsumeBufferBindingBase : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit AppendConsumeBufferBindingBase(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetIndexer& indexer) const final
	{
		const rdr::BufferView bufferView      = m_boundBuffer.GetBufferToBind();
		const rdr::BufferView countBufferView = m_boundCountBuffer.GetBufferToBind();

		bufferView.GetBuffer()->GetRHI().CopyUAVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), indexer[GetBaseBindingIdx()][0]);
		countBufferView.GetBuffer()->GetRHI().CopyUAVDescriptor(countBufferView.GetOffset(), countBufferView.GetSize(), indexer[GetBaseBindingIdx() + 1u][0]);
	}

	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		m_boundBuffer.AddRGDependency(builder, rg::ERGBufferAccess::ReadWrite);
		m_boundCountBuffer.AddRGDependency(builder, rg::ERGBufferAccess::ReadWrite);
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		lib::String bufferTypeName = isAppend ? lib::String("AppendStructuredBuffer") : lib::String("AppendStructuredBuffer");
		lib::String code;
		code += rdr::shader_translator::DefineType<TStorageType>();
		code += BuildBindingVariableCode(bufferTypeName + '<' + TStorageType::GetStructName() + "> " + name, bindingIdx);
		return code;
	}
	
	// We need to take two bindings - first for buffer and second for count (storage buffer)
	static constexpr std::array<rdr::ShaderBindingMetaData, 2> GetShaderBindingsMetaData()
	{
		return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::StorageBuffer, lib::Flags(smd::EBindingFlags::Storage, smd::EBindingFlags::Unbound)),
			rdr::ShaderBindingMetaData(rhi::EDescriptorType::StorageBuffer, smd::EBindingFlags::Storage) };
	}

	template<CInstanceOrRGBufferView TBufferType, CInstanceOrRGBufferView TCountBufferType>
	void Set(lib::AsConstParameter<TBufferType> buffer, lib::AsConstParameter<TCountBufferType> countBuffer)
	{
		Bool changed = false;

		if (m_boundBuffer != buffer)
		{
			m_boundBuffer.Set(buffer);
			changed = true;
		}
		if (m_boundCountBuffer != countBuffer)
		{
			m_boundCountBuffer.Set(countBuffer);
			changed = true;
		}

		if (changed)
		{
			MarkAsDirty();
		}
	}

	void Reset()
	{
		m_boundBuffer.Reset();
		m_boundCountBuffer.Reset();
	}

	template<CInstanceOrRGBufferView TType>
	lib::AsConstParameter<TType> GetBufferAs() const
	{
		return m_boundBuffer.GetAs<TType>();
	}

	template<CInstanceOrRGBufferView TType>
	lib::AsConstParameter<TType> GetCountBufferAs() const
	{
		return m_boundCountBuffer.GetAs<TType>();
	}

	Bool IsValid() const
	{
		return m_boundBuffer.IsValid() && m_boundCountBuffer.IsValid();
	}

protected:

	priv::BoundBufferVariant m_boundBuffer;
	priv::BoundBufferVariant m_boundCountBuffer;
};

} // priv

template<typename TStorageType>
using AppendStructuredBuffer = priv::AppendConsumeBufferBindingBase<TStorageType, true>;

template<typename TStorageType>
using ConsumeStructuredBuffer = priv::AppendConsumeBufferBindingBase<TStorageType, false>;

} // spt::gfx

