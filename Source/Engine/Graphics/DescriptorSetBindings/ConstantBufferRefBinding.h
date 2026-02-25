#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RHI/RHIBridge/RHILimitsImpl.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "RendererSettings.h"
#include "GPUApi.h"
#include "BufferBindingTypes.h"


namespace spt::gfx
{

template<typename TStruct, bool isOptional>
class GenericConstantBufferRefBinding : public rdr::DescriptorSetBinding 
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit GenericConstantBufferRefBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetIndexer& indexer) const final
	{
		if constexpr (!isOptional)
		{
			SPT_CHECK(IsValid());
		}

		if (IsValid())
		{
			const rdr::BufferView& bufferView = *m_boundBuffer.GetBufferToBind();
			bufferView.GetBuffer()->GetRHI().CopySRVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), indexer[GetBaseBindingIdx()][0]);
		}
	}

	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		if constexpr (!isOptional)
		{
			SPT_CHECK(IsValid());
		}

		if (IsValid())
		{
			m_boundBuffer.AddRGDependency(builder, rg::ERGBufferAccess::Read);
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return rdr::shader_translator::DefineType<TStruct>() + '\n' +
			BuildBindingVariableCode(lib::String("ConstantBuffer<") + rdr::shader_translator::GetTypeName<TStruct>() + "> " + name, bindingIdx);
	}

	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::None;

		if constexpr (isOptional)
		{
			lib::AddFlag(flags, smd::EBindingFlags::PartiallyBound);
		}

		return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::UniformBuffer, flags) };
	}

	template<priv::CInstanceOrRGBufferView TType>
	void Set(lib::AsConstParameter<TType> buffer)
	{
		if (m_boundBuffer != buffer)
		{
			if constexpr (std::is_same_v<TType, rdr::BufferView>)
			{
				SPT_CHECK(buffer.GetSize() == rdr::shader_translator::HLSLSizeOf<TStruct>());
			}
			else
			{
				SPT_CHECK(buffer->GetSize() == rdr::shader_translator::HLSLSizeOf<TStruct>());
			}

			m_boundBuffer.Set<TType>(buffer);
			MarkAsDirty();
		}
	}

	template<priv::CInstanceOrRGBufferView TType>
	GenericConstantBufferRefBinding& operator=(const TType& buffer)
	{
		Set<TType>(buffer);
		return *this;
	}

	void Reset()
	{
		m_boundBuffer.Reset();
		MarkAsDirty();
	}

	Bool IsValid() const
	{
		return m_boundBuffer.IsValid();
	}

	lib::SharedPtr<rdr::BindableBufferView> GetBoundBuffer() const
	{
		return m_boundBuffer.GetBoundBuffer();
	}

	rg::RGBufferViewHandle GetBoundRGBuffer() const
	{
		return m_boundBuffer.GetBoundRGBuffer();
	}

private:

	priv::BoundBufferVariant m_boundBuffer;
};


template<typename TStruct>
using ConstantBufferRefBinding = GenericConstantBufferRefBinding<TStruct, false>;

template<typename TStruct>
using OptionalConstantBufferRefBinding = GenericConstantBufferRefBinding<TStruct, true>;

} // spt::gfx
