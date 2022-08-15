#include "ShaderMetaDataBuilder.h"
#include "Common/CompiledShader.h"
#include "ShaderMetaDataBuilderTypes.h"
#include "ShaderMetaData.h"

#include "spirv_cross/spirv_cross.hpp"

namespace spt::sc
{

namespace priv
{

namespace helper
{

struct SpirvResourceData
{
	Uint8				m_setIdx;
	Uint8				m_bindingIdx;
	lib::HashedString	m_name;
};

static SpirvResourceData GetResourceData(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
{
	const Uint32 setIdx32				= compiler.get_decoration(resource.id, spv::Decoration::DecorationDescriptorSet);
	const Uint32 bindingIdx32			= compiler.get_decoration(resource.id, spv::Decoration::DecorationBinding);
	const lib::HashedString paramName	= resource.name;

	SPT_CHECK(setIdx32 <= smd::maxSetIdx);
	SPT_CHECK(bindingIdx32 <= smd::maxBindingIdx);
	SPT_CHECK(paramName.IsValid());

	const Uint8 setIdx		= static_cast<Uint8>(setIdx32);
	const Uint8 bindingIdx	= static_cast<Uint8>(bindingIdx32);

	return SpirvResourceData{ setIdx, bindingIdx, paramName };
}

} // helper

static void AddUniformBuffer(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& uniformBufferResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{

}

static void AddStorageBuffer(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& storageBufferResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{

}

static void AddCombinedTextureSampler(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& combinedTextureSamplerResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILE_FUNCTION();

	const auto [setIdx, bindingIdx, paramName] = helper::GetResourceData(compiler, combinedTextureSamplerResource);
	
	const smd::CombinedTextureSamplerBindingData combinedTextureSamplerBinding;
	outShaderMetaData.AddShaderBindingData(setIdx, bindingIdx, combinedTextureSamplerBinding);

	const smd::ShaderCombinedTextureSamplerParamEntry paramEntry(setIdx, bindingIdx);
	outShaderMetaData.AddShaderParamEntry(paramName, paramEntry);
}

static void AddTexture(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& textureResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILE_FUNCTION();

	const auto [setIdx, bindingIdx, paramName] = helper::GetResourceData(compiler, textureResource);
	
	const smd::TextureBindingData textureBinding;
	outShaderMetaData.AddShaderBindingData(setIdx, bindingIdx, textureBinding);

	const smd::ShaderTextureParamEntry paramEntry(setIdx, bindingIdx);
	outShaderMetaData.AddShaderParamEntry(paramName, paramEntry);
}

static void BuildShaderMetaData(const spirv_cross::Compiler& compiler, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILE_FUNCTION();

	const spirv_cross::ShaderResources resources = compiler.get_shader_resources();

	std::for_each(std::cbegin(resources.uniform_buffers), std::cend(resources.uniform_buffers),
		[&](const spirv_cross::Resource& uniformBuffer)
		{
			AddUniformBuffer(compiler, uniformBuffer, shaderStage, parametersMetaData, outShaderMetaData);
		});

	std::for_each(std::cbegin(resources.storage_buffers), std::cend(resources.storage_buffers),
		[&](const spirv_cross::Resource& storageBuffer)
		{
			AddStorageBuffer(compiler, storageBuffer, shaderStage, parametersMetaData, outShaderMetaData);
		});

	std::for_each(std::cbegin(resources.sampled_images), std::cend(resources.sampled_images),
		[&](const spirv_cross::Resource& combinedTextureSampler)
		{
			AddCombinedTextureSampler(compiler, combinedTextureSampler, shaderStage, parametersMetaData, outShaderMetaData);
		});

	std::for_each(std::cbegin(resources.storage_images), std::cend(resources.storage_images),
		[&](const spirv_cross::Resource& texture)
		{
			AddTexture(compiler, texture, shaderStage, parametersMetaData, outShaderMetaData);
		});
}

} // priv

void ShaderMetaDataBuilder::BuildShaderMetaData(const CompiledShader& shader, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILE_FUNCTION();

	const spirv_cross::Compiler compiler(shader.GetBinary());
	priv::BuildShaderMetaData(compiler, shader.GetStage(), parametersMetaData, outShaderMetaData);
}

}
