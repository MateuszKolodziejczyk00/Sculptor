#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"


namespace spt::gfx
{

template<typename TDescriptorSet>
class ChildDSBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit ChildDSBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		if (IsValid())
		{
			m_boundDS->UpdateDescriptors(context);
		}
	}
	
	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		if (IsValid())
		{
			m_boundDS->BuildRGDependencies(builder);
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		using DSHeadBindingType = typename TDescriptorSet::ReflHeadBindingType;
		return rdr::bindings_refl::BuildBindingsShaderCode<DSHeadBindingType>(bindingIdx);
	}
	
	static constexpr auto GetShaderBindingsMetaData()
	{
		return rdr::bindings_refl::GetShaderBindingsMetaData<TDescriptorSet>();
	}

	static void BuildAdditionalShaderCompilationArgs(rdr::ShaderCompilationAdditionalArgsBuilder& builder)
	{
		Super::BuildAdditionalShaderCompilationArgs(builder);
		builder.AddDescriptorSet(TDescriptorSet::GetDescriptorSetName());
		rdr::bindings_refl::BuildAdditionalShaderCompilationArgs<TDescriptorSet>(builder);
	}

	void Set(lib::MTHandle<TDescriptorSet> ds)
	{
		if(m_boundDS != ds)
		{
			m_boundDS = std::move(ds);
			MarkAsDirty();
		}
	}

	ChildDSBinding& operator=(lib::MTHandle<TDescriptorSet> ds)
	{
		Set(std::move(ds));
		return *this;
	}

	void Reset()
	{
		m_boundDS.Reset();
	}

	Bool IsValid() const
	{
		return m_boundDS.IsValid();
	}

private:

	lib::MTHandle<TDescriptorSet> m_boundDS;
};

} // spt::gfx
