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

	void Set(lib::SharedPtr<TDescriptorSet> ds)
	{
		if(m_boundDS != ds)
		{
			m_boundDS = std::move(ds);
			MarkAsDirty();
		}
	}

	void Set(const lib::SharedRef<TDescriptorSet>& ds)
	{
		const lib::SharedPtr<TDescriptorSet>& ptr = ds.ToSharedPtr();
		if(m_boundDS != ptr)
		{
			m_boundDS = std::move(ptr);
			MarkAsDirty();
		}
	}

	ChildDSBinding& operator=(lib::SharedPtr<TDescriptorSet> ds)
	{
		Set(std::move(ds));
		return *this;
	}

	ChildDSBinding& operator=(const lib::SharedRef<TDescriptorSet>& ds)
	{
		Set(ds);
		return *this;
	}

	void Reset()
	{
		m_boundDS.Reset();
	}

	Bool IsValid() const
	{
		return !!m_boundDS;
	}

private:

	lib::SharedPtr<TDescriptorSet> m_boundDS;
};

} // spt::gfx
