#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHISynchronizationTypes.h"
#include "Types/Buffer.h"


namespace spt::rdr
{

class CommandRecorder;

namespace utils
{

template<typename TStruct, typename TValue> requires std::is_constructible_v<TStruct, const TValue&>
lib::SharedRef<Buffer> CreateConstantBuffer(const RendererResourceName& name, const TValue& value)
{
	rhi::BufferDefinition bufferDef;
	bufferDef.usage = rhi::EBufferUsage::Uniform;
	bufferDef.size = sizeof(TStruct);
	const lib::SharedRef<Buffer> buffer = rdr::ResourcesManager::CreateBuffer(name, bufferDef, rhi::EMemoryUsage::CPUToGPU);

	rhi::RHIMappedBuffer<TStruct> mappedBuffer(buffer->GetRHI());

	new (mappedBuffer.Get()) TStruct(value);

	return buffer;
}

template<typename TStruct, typename TValue> requires std::is_constructible_v<TStruct, const TValue&>
BufferView CreateConstantBufferView(const RendererResourceName& name, const TValue& value)
{
	return CreateConstantBuffer<TStruct>(name, value)->CreateFullView();
}

} // utils

} // spt::rdr
