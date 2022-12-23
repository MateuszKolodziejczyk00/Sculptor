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

	explicit RWBufferBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateBuffer(GetName(), GetBufferToBind());
	}
	
	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		if (m_boundRGBuffer.IsValid())
		{
			builder.AddBufferAccess(m_boundRGBuffer, rg::ERGBufferAccess::ShaderReadWrite, GetShaderStageFlags());
		}
		else
		{
			builder.AddBufferAccess(m_boundBufferInstance, rg::ERGBufferAccess::ShaderReadWrite, GetShaderStageFlags());
		}
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
		if (m_boundBufferInstance != buffer.ToSharedPtr())
		{
			m_boundBufferInstance = buffer.ToSharedPtr();
			m_boundRGBuffer.Reset();
			MarkAsDirty();
		}
	}

	void Set(rg::RGBufferViewHandle buffer)
	{
		if (m_boundRGBuffer != buffer)
		{
			m_boundRGBuffer = buffer;
			m_boundBufferInstance.reset();
			MarkAsDirty();
		}
	}

	void Reset()
	{
		m_boundBufferInstance.reset();
		m_boundRGBuffer.Reset();
	}

	const lib::SharedPtr<rdr::BufferView>& GetBoundBufferInstance() const
	{
		return m_boundBufferInstance;
	}

	rg::RGBufferViewHandle GetBoundRGBuffer() const
	{
		return m_boundRGBuffer;
	}

	Bool IsValid() const
	{
		return !!m_boundBufferInstance || m_boundRGBuffer.IsValid();
	}

protected:

	lib::SharedRef<rdr::BufferView> GetBufferToBind() const
	{
		SPT_CHECK(IsValid());

		if (m_boundRGBuffer.IsValid())
		{
			SPT_CHECK(m_boundRGBuffer->IsAcquired());
			return lib::Ref(m_boundRGBuffer->GetBufferViewInstance());
		}
		else
		{
			return lib::Ref(m_boundBufferInstance);
		}
	}

	lib::SharedPtr<rdr::BufferView> m_boundBufferInstance;
	rg::RGBufferViewHandle			m_boundRGBuffer;
};


template<typename TStruct>
using RWStructuredBufferBinding = RWBufferBinding<TStruct>;

using RWByteAddressBuffer = RWBufferBinding<Byte>;

} // spt::gfx

