#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"


namespace spt::rdr
{

class BufferView;
class TextureView;


class RENDERER_TYPES_API DescriptorSetWriter
{
public:

	DescriptorSetWriter();

	void WriteBuffer(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const lib::SharedRef<BufferView>& bufferView);
	void WriteTexture(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const lib::SharedRef<TextureView>& textureView);

	void Reserve(SizeType writesNum);
	void Flush();

private:

	rhi::RHIDescriptorSetWriter m_rhiWriter;
};

} // spt::rdr