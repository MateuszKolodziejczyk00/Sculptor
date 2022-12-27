#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"
#include "BufferBindingTypes.h"


namespace spt::gfx
{

template<typename TStruct>
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
		m_boundBuffer.AddRGDependency(builder, rg::ERGBufferAccess::ShaderReadWrite, GetShaderStageFlags());
	}

	void CreateBindingMetaData(OUT smd::GenericShaderBinding& binding) const
	{
		binding.Set(smd::BufferBindingData(1, GetBindingFlags()));
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		if constexpr (IsByteBuffer())
		{
			return BuildBindingVariableCode(lib::String("RWByteAddressBuffer ") + name, bindingIdx);
		}
		else
		{
			return rdr::shader_translator::AddShaderStruct<TStruct>() + '\n' +
				BuildBindingVariableCode(lib::String("RWStructuredBuffer") + '<' + TStruct::GetStructName() + "> " + name, bindingIdx);
		}
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return lib::Flags(smd::EBindingFlags::Storage, smd::EBindingFlags::Unbound);
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


template<typename TStruct>
using RWStructuredBufferBinding = RWBufferBinding<TStruct>;

using RWByteAddressBuffer = RWBufferBinding<Byte>;

} // spt::gfx

