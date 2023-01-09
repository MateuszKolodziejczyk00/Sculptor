#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"
#include "BufferBindingTypes.h"


namespace spt::gfx
{

template<typename TStruct, Bool isUnbound, rg::ERGBufferAccess accessType>
class RWBufferBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit RWBufferBinding(const lib::HashedString& name)
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

	void CreateBindingMetaData(INOUT lib::DynamicArray<smd::GenericShaderBinding>& bindingsMetaData) const
	{
		smd::GenericShaderBinding& newBindingMetaData = bindingsMetaData.emplace_back();
		newBindingMetaData.Set(smd::BufferBindingData(1, GetBindingFlags()));
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		if constexpr (isUnbound)
		{
			if constexpr (IsByteBuffer())
			{
				return BuildBindingVariableCode(lib::String("RWByteAddressBuffer ") + name, bindingIdx);
			}
			else
			{
				return rdr::shader_translator::DefineType<TStruct>() + '\n' +
					BuildBindingVariableCode(lib::String("RWStructuredBuffer") + '<' + TStruct::GetStructName() + "> " + name, bindingIdx);
			}
		}
		else
			return rdr::shader_translator::DefineType<TStruct>() + '\n' +
				BuildBindingVariableCode(lib::String("RWBuffer") + '<' + TStruct::GetStructName() + "> " + name, bindingIdx);
		}
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::Storage;
		if (isUnbound)
		{
			lib::AddFlags(flags, smd::EBindingFlags::Unbound);
		}
		return isUnbound;
	}

	static constexpr Bool IsByteBuffer()
	{
		return std::is_same_v<TStruct, Byte>;
	}

	template<CInstanceOrRGBufferView TType>
	void Set(lib::AsConstParameter<TType> buffer)
	{
		SPT_CHECK(!!buffer);

		if (m_boundBuffer != buffer)
		{
			m_boundBuffer.Set<TType>(buffer);
			MarkAsDirty();
		}
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


template<typename TStruct, rg::ERGBufferAccess accessType>
using RWStructuredBufferBinding = RWBufferBinding<TStruct, true, accessType>;

template<rg::ERGBufferAccess accessType>
using RWByteAddressBuffer = RWBufferBinding<Byte, true, accessType>;

template<typename TStruct, rg::ERGBufferAccess accessType>
using RWBufferBinding = RWBufferBinding<TStruct, false, accessType>;

} // spt::gfx

