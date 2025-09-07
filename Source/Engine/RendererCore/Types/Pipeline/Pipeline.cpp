#include "Pipeline.h"
#include "Types/Shader.h"
#include "Utility/Templates/Overload.h"
#include "Types/Sampler.h"
#include "Types/DescriptorSetLayout.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "Types/DescriptorSetState/DescriptorManager.h"

namespace spt::rdr
{

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

	const DescriptorSetStateLayoutsRegistry& layoutsRegistry = DescriptorSetStateLayoutsRegistry::Get();

	rhi::PipelineLayoutDefinition layoutDefinition;

	const Uint32 descriptorSetsNum = m_metaData.GetDescriptorSetsNum();

	for (Uint32 dsIdx = 0; dsIdx < descriptorSetsNum; ++dsIdx)
	{
		rhi::RHIDescriptorSetLayout& rhiLayout = layoutDefinition.descriptorSetLayouts.emplace_back();

		if (m_metaData.HasValidDescriptorSetAtIndex(dsIdx))
		{
			const DSStateTypeID dsTypeID = DSStateTypeID(m_metaData.GetDescriptorSetStateTypeID(dsIdx));
			
			const lib::SharedPtr<DescriptorSetLayout>& layout = layoutsRegistry.GetLayoutChecked(dsTypeID);
			rhiLayout = layout->GetRHI();
		}
	}

	const Bool isBindless = true;
	if (isBindless)
	{
		SPT_CHECK(m_metaData.GetDescriptorSetStateTypeID(0u) == idxNone<Uint64>);

		if (layoutDefinition.descriptorSetLayouts.empty())
		{
			layoutDefinition.descriptorSetLayouts.resize(1u);
		}

		SPT_CHECK(!layoutDefinition.descriptorSetLayouts[0].IsValid());

		const lib::SharedPtr<DescriptorSetLayout>& bindlessLayout = Renderer::GetDescriptorManager().GetBindlessLayout();
		layoutDefinition.descriptorSetLayouts[0] = bindlessLayout->GetRHI();
	}
	
	return layoutDefinition;
}

} // spt::rdr
