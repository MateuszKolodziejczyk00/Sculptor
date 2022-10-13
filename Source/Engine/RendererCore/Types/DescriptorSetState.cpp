#include "DescriptorSetState.h"
#include "ShaderMetaData.h"
#include "DescriptorSetWriter.h"
#include "Buffer.h"

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
		const smd::BufferBindingData& bufferBinding = binding.As<smd::BufferBindingData>();

		rhi::WriteDescriptorDefinition writeDefinition;
		writeDefinition.bindingIdx		= bufferParam.bindingIdx;
		writeDefinition.arrayElement	= 0;
		writeDefinition.descriptorType	= bufferBinding.GetDescriptorType();

		SPT_CHECK(bufferBinding.IsUnbound() || bufferBinding.GetSize() <= buffer->GetSize());

		const Uint64 range = bufferBinding.IsUnbound() ? buffer->GetSize() : static_cast<Uint64>(bufferBinding.GetSize());
		SPT_CHECK(range > 0);

		m_writer.WriteBuffer(m_descriptorSet, writeDefinition, buffer, range);
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

DescriptorSetBinding::DescriptorSetBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag)
	: m_name(name)
	, m_descriptorDirtyFlag(descriptorDirtyFlag)
{ }

const lib::HashedString& DescriptorSetBinding::GetName() const
{
	return m_name;
}

void DescriptorSetBinding::MarkAsDirty()
{
	m_descriptorDirtyFlag = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetState ============================================================================

DescriptorSetState::DescriptorSetState(const RendererResourceName& name, EDescriptorSetStateFlags flags)
	: m_isDirty(false)
	, m_flags(flags)
	, m_id(utils::GenerateStateID())
	, m_descriptorSetHash(idxNone<SizeType>)
	, m_name(name)
{ }

DSStateID DescriptorSetState::GetID() const
{
	return m_id;
}

Bool DescriptorSetState::IsDirty() const
{
	return m_isDirty;
}

void DescriptorSetState::ClearDirtyFlag()
{
	m_isDirty = false;
}

EDescriptorSetStateFlags DescriptorSetState::GetFlags() const
{
	return m_flags;
}

const lib::DynamicArray<lib::HashedString>& DescriptorSetState::GetBindingNames() const
{
	return m_bindingNames;
}

SizeType DescriptorSetState::GetDescriptorSetHash() const
{
	return m_descriptorSetHash;
}

Uint32* DescriptorSetState::AddDynamicOffset()
{
	const Uint32* prevDataPtr = m_dynamicOffsets.data();

	Uint32& newOffset = m_dynamicOffsets.emplace_back(0);

	// make sure that array wasn't deallocated as this would result in dangling pointers in bindings to their offsets
	SPT_CHECK(prevDataPtr == m_dynamicOffsets.data());

	return &newOffset;
}

const lib::DynamicArray<Uint32>& DescriptorSetState::GetDynamicOffsets() const
{
	return m_dynamicOffsets;
}

const lib::HashedString& DescriptorSetState::GetName() const
{
	return m_name.Get();
}

void DescriptorSetState::SetBindingNames(lib::DynamicArray<lib::HashedString> inBindingNames)
{
	m_bindingNames = std::move(inBindingNames);
}

void DescriptorSetState::SetDescriptorSetHash(SizeType hash)
{
	m_descriptorSetHash = hash;
}

void DescriptorSetState::InitDynamicOffsetsArray(SizeType dynamicOffsetsNum)
{
	m_dynamicOffsets.reserve(dynamicOffsetsNum);
}

} // spt::rdr
