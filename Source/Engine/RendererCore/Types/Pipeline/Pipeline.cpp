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

static lib::SharedRef<Sampler> GetImmutableSamplerForBinding(const smd::SamplerBindingData& bindingMetaData)
{
	SPT_PROFILER_FUNCTION();

	const smd::EBindingFlags bindingFlags = bindingMetaData.flags;
	SPT_CHECK(lib::HasAnyFlag(bindingFlags, smd::EBindingFlags::ImmutableSampler));

	const rhi::SamplerDefinition& samplerDef = bindingMetaData.GetImmutableSamplerDefinition();
	return ResourcesManager::CreateSampler(samplerDef);
}

static void InitializeRHIBindingDefinition(Uint32 bindingIdx, const smd::GenericShaderBinding& bindingMetaData, rhi::DescriptorSetBindingDefinition& rhiBindingDef)
{
	// Optional per type initialization
	std::visit(
		lib::Overload
		{
			[&rhiBindingDef](const smd::TextureBindingData& textureBinding)
			{
			},
			[&rhiBindingDef](const smd::CombinedTextureSamplerBindingData& combinedTextureSamplerBinding)
			{
			},
			[&rhiBindingDef](const smd::BufferBindingData& bufferBinding)
			{
			},
			[&rhiBindingDef](const smd::SamplerBindingData& samplerBinding)
			{
				if (samplerBinding.IsImmutable())
				{
				    rhiBindingDef.immutableSampler = GetImmutableSamplerForBinding(samplerBinding)->GetRHI();
				}
			},
			[&rhiBindingDef](const smd::AccelerationStructureBindingData& bufferBinding)
			{
			}
		},
		bindingMetaData.GetBindingData());

	std::visit([&rhiBindingDef](const smd::CommonBindingData& commonBindingData)
			   {
				   rhiBindingDef.descriptorType = commonBindingData.GetDescriptorType();
 
				   rhiBindingDef.shaderStages = commonBindingData.GetShaderStages();

				   if (lib::HasAnyFlag(commonBindingData.flags, smd::EBindingFlags::PartiallyBound))
				   {
					   lib::AddFlags(rhiBindingDef.flags, rhi::EDescriptorSetBindingFlags::PartiallyBound, rhi::EDescriptorSetBindingFlags::UpdateAfterBind);
				   }

				   rhiBindingDef.descriptorCount = commonBindingData.elementsNum;
			   },
			   bindingMetaData.GetBindingData());

	rhiBindingDef.bindingIdx = bindingIdx;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pipeline ======================================================================================

Pipeline::Pipeline()
{ }

const smd::ShaderMetaData& Pipeline::GetMetaData() const
{
	return m_metaData;
}

void Pipeline::AppendToPipelineMetaData(const smd::ShaderMetaData& shaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	m_metaData.Append(shaderMetaData);
}

rhi::PipelineLayoutDefinition Pipeline::CreateLayoutDefinition() const
{
	SPT_PROFILER_FUNCTION();

	rhi::PipelineLayoutDefinition layoutDefinition;

	const smd::ShaderMetaData::DescriptorSetArray& descriptorSetArray = m_metaData.GetDescriptorSets();
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
