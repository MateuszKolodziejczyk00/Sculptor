#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"
#include "BufferBindingTypes.h"


namespace spt::gfx
{

template<typename TStruct, rg::ERGBufferAccess accessType, Bool isOptional>
class RWGenericBufferBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit RWGenericBufferBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetIndexer& indexer) const final
	{
		if constexpr (!isOptional)
		{
			SPT_CHECK_MSG(IsValid(), "Descriptor {} is not set", GetName().GetData());
		}

		if (IsValid())
		{
			const lib::SharedPtr<rdr::BindableBufferView>& bufferView = m_boundBuffer.GetBufferToBind();
			bufferView->GetBuffer()->GetRHI().CopyUAVDescriptor(bufferView->GetOffset(), bufferView->GetSize(), indexer[GetBaseBindingIdx()][0]);
		}
	}

	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		if (IsValid())
		{
			rg::RGBufferAccessInfo accessInfo;
			accessInfo.access = accessType;
			if constexpr (!std::is_same_v<TStruct, Byte>)
			{
				if (IsValid())
				{
					accessInfo.structTypeName = rdr::shader_translator::GetTypeName<TStruct>();
					accessInfo.elementsNum    = static_cast<Uint32>(m_boundBuffer.GetBoundBufferSize() / sizeof(rdr::HLSLStorage<TStruct>));
				}
			}
			m_boundBuffer.AddRGDependency(builder, accessInfo);
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		const lib::String prefix = accessType == rg::ERGBufferAccess::Read ? lib::String() : lib::String("RW");
		if constexpr (IsByteBuffer())
		{
			return BuildBindingVariableCode(prefix + lib::String("ByteAddressBuffer ") + name, bindingIdx);
		}
		else
		{
			return rdr::shader_translator::DefineType<TStruct>() + '\n' +
				BuildBindingVariableCode(prefix + lib::String("StructuredBuffer") + '<' + rdr::shader_translator::GetTypeName<TStruct>() + "> " + name, bindingIdx);
		}
	}
	
	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		smd::EBindingFlags flags = lib::Union(smd::EBindingFlags::Storage, smd::EBindingFlags::Unbound);

		if constexpr (isOptional)
		{
			lib::AddFlag(flags, smd::EBindingFlags::PartiallyBound);
		}

		return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::StorageBuffer, flags) };
	}

	static constexpr Bool IsByteBuffer()
	{
		return std::is_same_v<TStruct, Byte>;
	}

	template<priv::CInstanceOrRGBufferView TType>
	void Set(lib::AsConstParameter<TType> buffer)
	{
		if (m_boundBuffer != buffer)
		{
			m_boundBuffer.Set<TType>(buffer);
			MarkAsDirty();
		}
	}

	template<priv::CInstanceOrRGBufferView TType>
	RWGenericBufferBinding& operator=(const TType& buffer)
	{
		Set<TType>(buffer);
		return *this;
	}

	void Reset()
	{
		m_boundBuffer.Reset();
	}

	template<priv::CInstanceOrRGBufferView TType> 
	lib::AsConstParameter<TType> GetAs() const
	{
		return m_boundBuffer.GetAs<TType>();
	}

	Bool IsValid() const
	{
		return m_boundBuffer.IsValid();
	}

private:

	priv::BoundBufferVariant m_boundBuffer;
};


template<typename TStruct>
using RWStructuredBufferBinding = RWGenericBufferBinding<TStruct, rg::ERGBufferAccess::ReadWrite, false>;

template<typename TStruct>
using StructuredBufferBinding = RWGenericBufferBinding<TStruct, rg::ERGBufferAccess::Read, false>;

using RWByteAddressBuffer = RWGenericBufferBinding<Byte, rg::ERGBufferAccess::ReadWrite, false>;
using ByteAddressBuffer = RWGenericBufferBinding<Byte, rg::ERGBufferAccess::Read, false>;


template<typename TStruct>
using OptionalRWStructuredBufferBinding = RWGenericBufferBinding<TStruct, rg::ERGBufferAccess::ReadWrite, true>;

template<typename TStruct>
using OptionalStructuredBufferBinding = RWGenericBufferBinding<TStruct, rg::ERGBufferAccess::Read, true>;

using RWByteAddressBuffer = RWGenericBufferBinding<Byte, rg::ERGBufferAccess::ReadWrite, false>;
using ByteAddressBuffer = RWGenericBufferBinding<Byte, rg::ERGBufferAccess::Read, false>;

} // spt::gfx

