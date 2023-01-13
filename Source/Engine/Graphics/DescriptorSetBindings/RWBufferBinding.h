#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"
#include "BufferBindingTypes.h"


namespace spt::gfx
{

template<typename TStruct, rg::ERGBufferAccess accessType>
class RWGenericBufferBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit RWGenericBufferBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateBuffer(GetName(), m_boundBuffer.GetBufferToBind());
	}
	
	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		m_boundBuffer.AddRGDependency(builder, accessType, GetShaderStageFlags());
	}

	static void CreateBindingMetaData(INOUT lib::DynamicArray<smd::GenericShaderBinding>& bindingsMetaData)
	{
		smd::GenericShaderBinding& newBindingMetaData = bindingsMetaData.emplace_back();
		newBindingMetaData.Set(smd::BufferBindingData(1, GetBindingFlags()));
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

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::Storage;
		lib::AddFlags(flags, smd::EBindingFlags::Unbound);
		return flags;
	}

	static constexpr Bool IsByteBuffer()
	{
		return std::is_same_v<TStruct, Byte>;
	}

	template<CInstanceOrRGBufferView TType>
	void Set(lib::AsConstParameter<TType> buffer)
	{
		if (m_boundBuffer != buffer)
		{
			m_boundBuffer.Set<TType>(buffer);
			MarkAsDirty();
		}
	}

	template<CInstanceOrRGBufferView TType>
	RWGenericBufferBinding& operator=(const TType& buffer)
	{
		Set<TType>(buffer);
		return *this;
	}

	void Reset()
	{
		m_boundBuffer.Reset();
	}

	template<CInstanceOrRGBufferView TType> 
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
using RWStructuredBufferBinding = RWGenericBufferBinding<TStruct, rg::ERGBufferAccess::ReadWrite>;

template<typename TStruct>
using StructuredBufferBinding = RWGenericBufferBinding<TStruct, rg::ERGBufferAccess::Read>;

using RWByteAddressBuffer = RWGenericBufferBinding<Byte, rg::ERGBufferAccess::ReadWrite>;
using ByteAddressBuffer = RWGenericBufferBinding<Byte, rg::ERGBufferAccess::Read>;

} // spt::gfx

