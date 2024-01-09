#include "DescriptorSetWriter.h"
#include "Buffer.h"
#include "Texture.h"
#include "AccelerationStructure.h"

namespace spt::rdr
{

DescriptorSetWriter::DescriptorSetWriter()
{ }

void DescriptorSetWriter::WriteBuffer(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const BufferView& bufferView, Uint64 range)
{
	const lib::SharedRef<Buffer> buffer = bufferView.GetBuffer();

	SPT_CHECK(range <= bufferView.GetSize());
	
	m_rhiWriter.WriteBuffer(set, writeDef, buffer->GetRHI(), bufferView.GetOffset(), range);
}

void DescriptorSetWriter::WriteBuffer(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const BufferView& bufferView, Uint64 range, const BufferView& countBufferView)
{
	const lib::SharedRef<Buffer> buffer = bufferView.GetBuffer();
	SPT_CHECK(range <= bufferView.GetSize());
	
	const lib::SharedRef<Buffer> countBuffer = bufferView.GetBuffer();
	SPT_CHECK(sizeof(Int32) <= countBufferView.GetSize());
	
	m_rhiWriter.WriteBuffer(set, writeDef, buffer->GetRHI(), bufferView.GetOffset(), range, countBuffer->GetRHI(), countBufferView.GetOffset());
}

void DescriptorSetWriter::WriteTexture(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const lib::SharedRef<TextureView>& textureView)
{
	m_rhiWriter.WriteTexture(set, writeDef, textureView->GetRHI());
}

void DescriptorSetWriter::WriteAccelerationStructure(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const lib::SharedRef<TopLevelAS>& tlas)
{
	m_rhiWriter.WriteAccelerationStructure(set, writeDef, tlas->GetRHI());
}

void DescriptorSetWriter::Reserve(SizeType writesNum)
{
	m_rhiWriter.Reserve(writesNum);
}

void DescriptorSetWriter::Flush()
{
	m_rhiWriter.Flush();
}

} // spt::rdr
