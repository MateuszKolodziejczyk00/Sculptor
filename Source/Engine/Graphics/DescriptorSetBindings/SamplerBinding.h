#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RHICore/RHISamplerTypes.h"


namespace spt::gfx
{

template<rhi::SamplerDefinition definition>
class ImmutableSamplerBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit ImmutableSamplerBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		// Do nothing
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return BuildBindingVariableCode(lib::String("SamplerState ") + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::ImmutableSampler;
		SPT_STATIC_CHECK(definition.addressingModeU == definition.addressingModeV && definition.addressingModeU == definition.addressingModeW);
		SPT_STATIC_CHECK(definition.addressingModeU != rhi::EAxisAddressingMode::MorroredClampToEdge); // Not supported

		constexpr rhi::EAxisAddressingMode addressingMode = definition.addressingModeU;

		if constexpr (addressingMode == rhi::EAxisAddressingMode::ClampToEdge || addressingMode == rhi::EAxisAddressingMode::ClampToBorder)
		{
			lib::AddFlag(flags, smd::EBindingFlags::ClampAddressing);
			if constexpr (addressingMode == rhi::EAxisAddressingMode::ClampToBorder)
			{
				lib::AddFlag(flags, smd::EBindingFlags::ClampToBorder);
			}
		}
		else // repeat
		{
			if constexpr (addressingMode == rhi::EAxisAddressingMode::MirroredRepeat)
			{

				lib::AddFlag(flags, smd::EBindingFlags::MirroredAddressing);
			}
		}

		if constexpr (definition.mipMapAdressingMode == rhi::EMipMapAddressingMode::Linear)
		{
			lib::AddFlag(flags, smd::EBindingFlags::MipMapsLinear);
		}

		SPT_STATIC_CHECK(definition.magnificationFilter == definition.minificationFilter);
		if constexpr (definition.magnificationFilter == rhi::ESamplerFilterType::Linear)
		{
			lib::AddFlag(flags, smd::EBindingFlags::FilterLinear);
		}

		if constexpr (definition.enableAnisotropy)
		{
			lib::AddFlag(flags, smd::EBindingFlags::EnableAnisotropy);
		}

		constexpr rhi::EBorderColor borderColor = definition.borderColor;
		constexpr Bool intBorder = borderColor == rhi::EBorderColor::IntOpaqueBlack
								|| borderColor == rhi::EBorderColor::IntOpaqueWhite
								|| borderColor == rhi::EBorderColor::IntTransparentBlack;

		constexpr Bool whiteBorder = borderColor == rhi::EBorderColor::FloatOpaqueWhite
								  || borderColor == rhi::EBorderColor::IntOpaqueWhite;

		constexpr Bool transparentBorder = borderColor == rhi::EBorderColor::FloatTransparentBlack
										|| borderColor == rhi::EBorderColor::IntTransparentBlack;

		if constexpr (intBorder)
		{
			lib::AddFlags(flags, smd::EBindingFlags::IntBorder);
		}

		if constexpr (whiteBorder)
		{
			lib::AddFlag(flags, smd::EBindingFlags::WhiteBorder);
		}

		if constexpr (transparentBorder)
		{
			lib::AddFlag(flags, smd::EBindingFlags::TransparentBorder);
		}

		return flags;
	}
};

} // spt::gfx