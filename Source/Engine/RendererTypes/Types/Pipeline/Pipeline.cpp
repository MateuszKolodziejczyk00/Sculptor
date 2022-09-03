#include "Pipeline.h"
#include "Types/Shader.h"
#include "Utility/Templates/Overload.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// priv ==========================================================================================

namespace priv
{

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

	std::visit(
		[&rhiBindingDef](const smd::CommonBindingData& commonBindingData)
		{
			rhiBindingDef.shaderStages = commonBindingData.GetShaderStages();
		},
		bindingMetaData.GetBindingData());

	rhiBindingDef.bindingIdx = bindingIdx;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pipeline ======================================================================================

Pipeline::Pipeline(const lib::SharedPtr<Shader>& shader)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!!shader);

	m_metaData = shader->GetMetaData();
}

const lib::SharedPtr<smd::ShaderMetaData>& Pipeline::GetMetaData() const
{
	return m_metaData;
}

rhi::PipelineLayoutDefinition Pipeline::CreateLayoutDefinition(const smd::ShaderMetaData& metaData) const
{
	SPT_PROFILE_FUNCTION();

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

} // spt::renderer
