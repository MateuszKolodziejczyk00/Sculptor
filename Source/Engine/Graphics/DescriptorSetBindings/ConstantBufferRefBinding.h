#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RHI/RHIBridge/RHILimitsImpl.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "RendererSettings.h"
#include "Renderer.h"
#include "BufferBindingTypes.h"


namespace spt::gfx
{

template<typename TStruct>
class ConstantBufferRefBinding : public rdr::DescriptorSetBinding 
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit ConstantBufferRefBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateBuffer(GetName(), m_boundBuffer.GetBufferToBind());
	}
	
	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		m_boundBuffer.AddRGDependency(builder, rg::ERGBufferAccess::Read);
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return rdr::shader_translator::DefineType<TStruct>() + '\n' +
			BuildBindingVariableCode(lib::String("ConstantBuffer<") + rdr::shader_translator::GetTypeName<TStruct>() + "> " + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return smd::EBindingFlags::None;
	}

	template<priv::CInstanceOrRGBufferView TType>
	void Set(lib::AsConstParameter<TType> buffer)
	{
		if (m_boundBuffer != buffer)
		{
			if constexpr (std::is_same_v<TType, rdr::BufferView>)
			{
				SPT_CHECK(buffer.GetSize() == sizeof(TStruct));
			}
			else
			{
				SPT_CHECK(buffer->GetSize() == sizeof(TStruct));
			}

			m_boundBuffer.Set<TType>(buffer);
			MarkAsDirty();
		}
	}

	template<priv::CInstanceOrRGBufferView TType>
	ConstantBufferRefBinding& operator=(const TType& buffer)
	{
		Set<TType>(buffer);
		return *this;
	}

private:

	priv::BoundBufferVariant m_boundBuffer;

};

} // spt::gfx
