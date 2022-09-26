#include "DescriptorSetsManager.h"
#include "Types/Pipeline/Pipeline.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetState.h"

namespace spt::rdr
{

DescriptorSetsManager::DescriptorSetsManager()
{ }

void DescriptorSetsManager::Initialize()
{
	rhi::RHIDescriptorSetManager::GetInstance().InitializeRHI();
}

void DescriptorSetsManager::Uninitialize()
{
	rhi::RHIDescriptorSetManager::GetInstance().ReleaseRHI();
}

void DescriptorSetsManager::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	m_persistentDescriptorSets.UpdatePersistentDescriptors();
}

rhi::RHIDescriptorSet DescriptorSetsManager::GetDescriptorSet(const lib::SharedPtr<Pipeline>& pipeline, const lib::SharedPtr<DescriptorSetState>& descriptorSetState, Uint32& outDescriptorSetIdx)
{
	SPT_PROFILER_FUNCTION();

	if (lib::HasAnyFlag(descriptorSetState->GetFlags(), EDescriptorSetStateFlags::Persistent))
	{
		outDescriptorSetIdx = GetDescriptorSetIdxInPipeline(pipeline, descriptorSetState);
		SPT_CHECK(outDescriptorSetIdx != idxNone<Uint32>);

		return m_persistentDescriptorSets.GetOrCreateDescriptorSet(pipeline, descriptorSetState, outDescriptorSetIdx);
	}
	else
	{
		SPT_CHECK_NO_ENTRY();
		return rhi::RHIDescriptorSet();
	}
}

Uint32 DescriptorSetsManager::GetDescriptorSetIdxInPipeline(const lib::SharedPtr<Pipeline>& pipeline, const lib::SharedRef<DescriptorSetState>& state) const
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<smd::ShaderMetaData> metaData = pipeline->GetMetaData();

	const lib::DynamicArray<lib::HashedString>& descriptorSetParamNames = state->GetBindingNames();
	SPT_CHECK(!descriptorSetParamNames.empty());

	const smd::ShaderParamEntryCommon firstParam = metaData->FindParamEntry(descriptorSetParamNames[0]);
	SPT_CHECK(firstParam.IsValid());
	const Uint32 bindingIdx = static_cast<Uint32>(firstParam.bindingIdx);

	SPT_CHECK(std::all_of(std::cbegin(descriptorSetParamNames), std::cend(descriptorSetParamNames),
						  [bindingIdx, &metaData](const lib::HashedString& paramName)
						  {
							  const smd::ShaderParamEntryCommon paramEntry = metaData->FindParamEntry(paramName);
							  SPT_CHECK(paramEntry.IsValid());
							  return static_cast<Uint32>(paramEntry.bindingIdx) == bindingIdx;
						  }));

	return bindingIdx;
}

} // spt::rdr
