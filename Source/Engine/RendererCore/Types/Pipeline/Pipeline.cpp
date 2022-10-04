#include "Pipeline.h"
#include "Types/Shader.h"
#include "Utility/Templates/Overload.h"
#include "Types/Sampler.h"
#include "ResourcesManager.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// priv ==========================================================================================

namespace priv
{

static rhi::EBorderColor GetBorderColor(smd::EBindingFlags bindingFlags)
{
	const Bool intBorder			= lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::IntBorder);
	const Bool whiteBorder			= lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::WhiteBorder);
	const Bool transparentBorder	= lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::TransparentBorder);

	if (intBorder)
	{
		if (whiteBorder)
		{
			SPT_CHECK(!transparentBorder);
			return rhi::EBorderColor::IntOpaqueWhite;
		}
		else
		{
			return transparentBorder ? rhi::EBorderColor::IntTransparentBlack : rhi::EBorderColor::IntOpaqueBlack;
		}
	}
	else
	{
		if (whiteBorder)
		{
			SPT_CHECK(!transparentBorder);
			return rhi::EBorderColor::FloatOpaqueWhite;
		}
		else
		{
			return transparentBorder ? rhi::EBorderColor::FloatTransparentBlack : rhi::EBorderColor::FloatOpaqueBlack;
		}
	}
}

static rhi::EAxisAddressingMode GetAxisAddressingMode(smd::EBindingFlags bindingFlags)
{
	if (lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::ClampAddressing))
	{
		if (lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::ClampToBorder))
		{
			return rhi::EAxisAddressingMode::ClampToBorder;
		}
		else
		{
			return rhi::EAxisAddressingMode::ClampToEdge;
		}
	}
	else
	{
		if (lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::MirroredAddressing))
		{
			return rhi::EAxisAddressingMode::MirroredRepeat;
		}
		else
		{
			return rhi::EAxisAddressingMode::Repeat;
		}
	}
}

static lib::SharedRef<Sampler> GetImmutableSamplerForBinding(const smd::CommonBindingData& bindingMetaData)
{
	SPT_PROFILER_FUNCTION();

	const smd::EBindingFlags bindingFlags = bindingMetaData.flags;

	SPT_CHECK(lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::ImmutableSampler));

	rhi::SamplerDefinition samplerDef;
	lib::AddFlag(samplerDef.flags, rhi::ESamplerFlags::Persistent);
	samplerDef.minificationFilter	= lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::FilterLinear) ? rhi::ESamplerFilterType::Linear : rhi::ESamplerFilterType::Nearest;
	samplerDef.magnificationFilter	= samplerDef.minificationFilter;
	samplerDef.mipMapAdressingMode	= lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::MipMapsLinear) ? rhi::EMipMapAddressingMode::Linear : rhi::EMipMapAddressingMode::Nearest;
	samplerDef.addressingModeU		= GetAxisAddressingMode(bindingFlags);
	samplerDef.addressingModeV		= samplerDef.addressingModeW = samplerDef.addressingModeU;
	samplerDef.enableAnisotropy		= lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::EnableAnisotropy);
	samplerDef.maxAnisotropy		= samplerDef.enableAnisotropy ? 16.f : 0.f;
	samplerDef.unnormalizedCoords	= lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::UnnormalizedCoords);
	samplerDef.borderColor			= GetBorderColor(bindingFlags);

	return ResourcesManager::CreateSampler(samplerDef);
}

static void InitializeRHIBindingDefinition(Uint32 bindingIdx, const smd::GenericShaderBinding& bindingMetaData, rhi::DescriptorSetBindingDefinition& rhiBindingDef)
{
	constexpr static Uint32 unboundDescriptorSetNum = 1024;

	std::visit(
		lib::Overload
		{
			[&rhiBindingDef](const smd::TextureBindingData& textureBinding)
			{
				const Bool isStorageData = lib::HasAnyFlag(textureBinding.flags, smd::EBindingFlags::Storage);

				rhiBindingDef.descriptorType = isStorageData ? rhi::EDescriptorType::StorageTexture : rhi::EDescriptorType::SampledTexture;

				if (lib::HasAnyFlag(textureBinding.flags, smd::EBindingFlags::Unbound))
				{
					lib::AddFlags(rhiBindingDef.flags, rhi::EDescriptorSetBindingFlags::PartiallyBound, rhi::EDescriptorSetBindingFlags::UpdateAfterBind);
					rhiBindingDef.descriptorCount = unboundDescriptorSetNum;
				}
				else
				{
					rhiBindingDef.descriptorCount = 1;
				}
			},
			[&rhiBindingDef](const smd::CombinedTextureSamplerBindingData& combinedTextureSamplerBinding)
			{
				rhiBindingDef.descriptorType = rhi::EDescriptorType::CombinedTextureSampler;

				if (lib::HasAnyFlag(combinedTextureSamplerBinding.flags, smd::EBindingFlags::Unbound))
				{
					lib::AddFlags(rhiBindingDef.flags, rhi::EDescriptorSetBindingFlags::PartiallyBound, rhi::EDescriptorSetBindingFlags::UpdateAfterBind);
					rhiBindingDef.descriptorCount = unboundDescriptorSetNum;
				}
				else
				{
					rhiBindingDef.descriptorCount = 1;
				}
			},
			[&rhiBindingDef](const smd::BufferBindingData& bufferBinding)
			{
				const Bool isStorageData = lib::HasAnyFlag(bufferBinding.flags, smd::EBindingFlags::Storage);

				if (lib::HasAnyFlag(bufferBinding.flags, smd::EBindingFlags::DynamicOffset))
				{
					rhiBindingDef.descriptorType = isStorageData ? rhi::EDescriptorType::StorageBufferDynamicOffset : rhi::EDescriptorType::UniformBufferDynamicOffset;
				}
				else if (lib::HasAnyFlag(bufferBinding.flags, smd::EBindingFlags::TexelBuffer))
				{
					rhiBindingDef.descriptorType = isStorageData ? rhi::EDescriptorType::StorageTexelBuffer : rhi::EDescriptorType::UniformTexelBuffer;
				}
				else
				{
					rhiBindingDef.descriptorType = isStorageData ? rhi::EDescriptorType::StorageBuffer : rhi::EDescriptorType::UniformBuffer;
				}

				rhiBindingDef.descriptorCount = 1;
			}
		},
		bindingMetaData.GetBindingData());

	std::visit([&rhiBindingDef](const smd::CommonBindingData& commonBindingData)
			   {
				   rhiBindingDef.shaderStages = commonBindingData.GetShaderStages();

				   if (lib::HasAnyFlag(commonBindingData.flags, smd::EBindingFlags::ImmutableSampler))
				   {
					   rhiBindingDef.immutableSampler = GetImmutableSamplerForBinding(commonBindingData)->GetRHI();
				   }
			   },
			   bindingMetaData.GetBindingData());

	rhiBindingDef.bindingIdx = bindingIdx;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pipeline ======================================================================================

Pipeline::Pipeline(const lib::SharedRef<Shader>& shader)
	: m_metaData(shader->GetMetaData())
{ }

const lib::SharedRef<smd::ShaderMetaData>& Pipeline::GetMetaData() const
{
	return m_metaData;
}

rhi::PipelineLayoutDefinition Pipeline::CreateLayoutDefinition(const smd::ShaderMetaData& metaData) const
{
	SPT_PROFILER_FUNCTION();

	rhi::PipelineLayoutDefinition layoutDefinition;

	const smd::ShaderMetaData::DescriptorSetArray& descriptorSetArray = m_metaData->GetDescriptorSets();
	std::transform(std::cbegin(descriptorSetArray), std::cend(descriptorSetArray), std::back_inserter(layoutDefinition.descriptorSets),
		[](const smd::ShaderDescriptorSet& descriptorSetMetaData)
		{
			rhi::DescriptorSetDefinition pipelineDSDefinition;

			const lib::DynamicArray<smd::GenericShaderBinding>& shaderBindingsMetaData = descriptorSetMetaData.GetBindings();
			for (SizeType idx = 0; idx < shaderBindingsMetaData.size(); ++idx)
			{
				rhi::DescriptorSetBindingDefinition bindingDef;
				priv::InitializeRHIBindingDefinition(static_cast<Uint32>(idx), shaderBindingsMetaData[idx], bindingDef);

				pipelineDSDefinition.bindings.emplace_back(bindingDef);
			}

			return pipelineDSDefinition;
		});

	return layoutDefinition;
}

} // spt::rdr
