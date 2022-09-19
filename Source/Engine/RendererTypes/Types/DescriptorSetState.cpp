#include "DescriptorSetState.h"
#include "ShaderMetaData.h"
#include "DescriptorSetWriter.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

namespace utils
{

static DSStateID GenerateStateID()
{
	static DSStateID current = 1;
	return current++;
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetUpdateContext ====================================================================

DescriptorSetUpdateContext::DescriptorSetUpdateContext(rhi::RHIDescriptorSet descriptorSet, DescriptorSetWriter& writer, const lib::SharedRef<smd::ShaderMetaData>& metaData)
	: m_descriptorSet(descriptorSet)
	, m_writer(writer)
	, m_metaData(metaData)
{ }

void DescriptorSetUpdateContext::UpdateBuffer(const lib::HashedString& name, const lib::SharedRef<BufferView>& buffer) const
{
	SPT_PROFILER_FUNCTION();

	const smd::ShaderBufferParamEntry bufferParam = m_metaData->FindParamEntry<smd::ShaderBufferParamEntry>(name);
	
	if (bufferParam.IsValid())
	{
		const smd::GenericShaderBinding binding = m_metaData->GetBindingData(bufferParam);

		rhi::WriteDescriptorDefinition writeDefinition;
		writeDefinition.bindingIdx		= bufferParam.bindingIdx;
		writeDefinition.arrayElement	= 0;
		writeDefinition.descriptorType	= binding.GetDescriptorType();

		m_writer.WriteBuffer(m_descriptorSet, writeDefinition, buffer);
	}
}

void DescriptorSetUpdateContext::UpdateTexture(const lib::HashedString& name, const lib::SharedRef<TextureView>& texture) const
{
	SPT_PROFILER_FUNCTION();

	const smd::ShaderTextureParamEntry textureParam = m_metaData->FindParamEntry<smd::ShaderTextureParamEntry>(name);

	if (textureParam.IsValid())
	{
		const smd::GenericShaderBinding& binding = m_metaData->GetBindingData(textureParam);

		rhi::WriteDescriptorDefinition writeDefinition;
		writeDefinition.bindingIdx		= textureParam.bindingIdx;
		writeDefinition.arrayElement	= 0;
		writeDefinition.descriptorType	= binding.GetDescriptorType();

		m_writer.WriteTexture(m_descriptorSet, writeDefinition, texture);
	}
}

const smd::ShaderMetaData& DescriptorSetUpdateContext::GetMetaData() const
{
	return *m_metaData;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetState ============================================================================

DescriptorSetState::DescriptorSetState()
	: m_state(utils::GenerateStateID())
	, m_isDirty(false)
{ }

DSStateID DescriptorSetState::GetID() const
{
	return m_state;
}

Bool DescriptorSetState::IsDirty() const
{
	return m_isDirty;
}

void DescriptorSetState::ClearDirtyFlag()
{
	m_isDirty = false;
}

void DescriptorSetState::MarkAsDirty()
{
	m_isDirty = true;
}

} // spt::rdr
