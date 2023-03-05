#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"


namespace spt::rdr
{

class BufferView;
class TextureView;
class TopLevelAS;


class RENDERER_CORE_API DescriptorSetWriter
{
public:

	DescriptorSetWriter();

	void WriteBuffer(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const BufferView& bufferView, Uint64 range);
	void WriteBuffer(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const BufferView& bufferView, Uint64 range, const BufferView& countBufferView);
	void WriteTexture(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const lib::SharedRef<TextureView>& textureView);
	void WriteAccelerationStructure(const rhi::RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const lib::SharedRef<TopLevelAS>& tlas);

	void Reserve(SizeType writesNum);
	void Flush();

private:

	rhi::RHIDescriptorSetWriter m_rhiWriter;
};

} // spt::rdr