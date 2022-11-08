#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RHICore/RHITextureTypes.h"
#include "RGResources/RenderGraphResource.h"


namespace spt::rg
{

struct RGTextureAccessDef
{
	RGTextureView	textureView;
	ERGAccess		access;
};


struct RGBufferAccessDef
{
	RGBufferHandle	resource;
	ERGAccess		access;
};


class RGDependenciesBuilder
{
public:

	RGDependenciesBuilder() = default;

	void AddTextureAccess(RGTextureView texture, ERGAccess access)
	{
		m_textureAccesses.emplace_back(RGTextureAccessDef{ texture, access });
	}

	void AddBufferAccess(RGBufferHandle buffer, ERGAccess access)
	{
		m_bufferAccesses.emplace_back(RGBufferAccessDef{ buffer, access });
	}
	
	const lib::DynamicArray<RGTextureAccessDef>& GetTextureAccesses() const
	{
		return m_textureAccesses;
	}

	const lib::DynamicArray<RGBufferAccessDef>& GetBufferAccesses() const
	{
		return m_bufferAccesses;
	}

private:

	lib::DynamicArray<RGTextureAccessDef>  m_textureAccesses;
	lib::DynamicArray<RGBufferAccessDef>  m_bufferAccesses;
};


template<typename TInstanceType>
class RGDescriptorSetState abstract : public rdr::DescriptorSetState
{
public:

	RGDescriptorSetState() = default;

	void BuildRGDependencies(RGDependenciesBuilder& builder)
	{
		TInstanceType& self = SelfAsInstance();
		const auto& bindingsBegin = self.GetBindingsBegin();
		rdr::bindings_refl::ForEachBinding(bindingsBegin, [&builder]<typename TBindingHandle>(const TBindingHandle& bindingHandle)
		{
			using BindingType = typename TBindingHandle::BindingType;

			constexpr Bool supportsRG = (requires(const BindingType& binding) { binding.BuildRGDependencies(std::declval<RGDependenciesBuilder>()); });

			SPT_STATIC_CHECK_MSG(supportsRG, "binding doesn't support Render Graph");
			if constexpr (supportsRG)
			{
				const BindingType& binding = bindingHandle.GetBinding();
				binding.BuildRGDependencies(builder);
			}
		});
	}

private:

	TInstanceType& SelfAsInstance()
	{
		return *static_cast<TInstanceType*>(this);
	}
};

} // spt::rg