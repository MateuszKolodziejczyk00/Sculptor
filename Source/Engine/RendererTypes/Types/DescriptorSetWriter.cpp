#include "DescriptorSetWriter.h"
#include "Buffer.h"
#include "Texture.h"

namespace spt::rdr
{

DescriptorSetWriter::DescriptorSetWriter()
{ }

void DescriptorSetWriter::WriteBuffer(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const lib::SharedRef<BufferView>& bufferView)
{
	lib::SharedPtr<const Buffer> buffer = bufferView->GetBuffer();
	SPT_CHECK(!!buffer);
	
	m_rhiWriter.WriteBuffer(set, writeDef, buffer->GetRHI(), bufferView->GetOffset(), bufferView->GetSize());
}

void DescriptorSetWriter::WriteTexture(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const lib::SharedRef<TextureView>& textureView)
{
	m_rhiWriter.WriteTexture(set, writeDef, textureView->GetRHI());
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
