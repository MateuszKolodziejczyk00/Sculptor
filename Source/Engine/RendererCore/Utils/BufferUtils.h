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
lib::SharedRef<Buffer> CreateDataBuffer(const RendererResourceName& name, const TValue& value, rhi::EBufferUsage usage)
{
	rhi::BufferDefinition bufferDef;
	bufferDef.usage = usage;
	bufferDef.size  = sizeof(rdr::HLSLStorage<TStruct>);
	const lib::SharedRef<Buffer> buffer = rdr::ResourcesManager::CreateBuffer(name, bufferDef, rhi::EMemoryUsage::CPUToGPU);

	rhi::RHIMappedBuffer<TStruct> mappedBuffer(buffer->GetRHI());

	new (mappedBuffer.Get()) rdr::HLSLStorage<TStruct>(value);

	return buffer;
}

template<typename TStruct, typename TValue> requires std::is_constructible_v<TStruct, const TValue&>
lib::SharedPtr<rdr::BindableBufferView> CreateConstantBufferView(const RendererResourceName& name, const TValue& value)
{
	return CreateDataBuffer<TStruct>(name, value, rhi::EBufferUsage::Uniform)->GetFullView();
}

template<typename TStruct, typename TValue> requires std::is_constructible_v<TStruct, const TValue&>
lib::SharedPtr<rdr::BindableBufferView> CreateStorageBufferView(const RendererResourceName& name, const TValue& value)
{
	return CreateDataBuffer<TStruct>(name, value, rhi::EBufferUsage::Storage)->GetFullView();
}


} // utils

} // spt::rdr
