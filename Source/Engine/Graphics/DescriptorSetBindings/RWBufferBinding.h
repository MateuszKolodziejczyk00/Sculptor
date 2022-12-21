#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"


namespace spt::gfx
{

template<typename TStruct>
class RWBufferBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit RWBufferBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag)
		: Super(name, descriptorDirtyFlag)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateBuffer(GetName(), GetBufferToBind());
	}
	
	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		SPT_CHECK_NO_ENTRY();
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

	void Set(const lib::SharedRef<rdr::BufferView>& buffer)
	{
		m_boundBuffer = buffer.ToSharedPtr();
	}

	void Reset()
	{
		m_boundBuffer.reset();
	}

	const lib::SharedPtr<rdr::BufferView>& GetBoundBufferInstance() const
	{
		return m_boundBuffer;
	}

	Bool IsValid() const
	{
		return !!m_boundBuffer;
	}

protected:

	lib::SharedRef<rdr::BufferView> GetBufferToBind() const
	{
		SPT_CHECK(!!m_boundBuffer);

		return lib::Ref(m_boundBuffer);
	}

	lib::SharedPtr<rdr::BufferView> m_boundBuffer;
};


template<typename TStruct>
using RWStructuredBufferBinding = RWBufferBinding<TStruct>;

using RWByteAddressBuffer = RWBufferBinding<Byte>;

} // spt::gfx

