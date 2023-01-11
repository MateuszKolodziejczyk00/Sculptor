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
	Uint32				setIdx;
	Uint32				bindingIdx;
	lib::HashedString	name;
};

static SpirvResourceData GetResourceData(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 setIdx					= compiler.get_decoration(resource.id, spv::Decoration::DecorationDescriptorSet);
	const Uint32 bindingIdx				= compiler.get_decoration(resource.id, spv::Decoration::DecorationBinding);
	const lib::HashedString paramName	= compiler.get_name(resource.id);

	SPT_CHECK(setIdx <= smd::maxSetIdx);
	SPT_CHECK(bindingIdx <= smd::maxBindingIdx);
	SPT_CHECK(paramName.IsValid());

	return SpirvResourceData{ setIdx, bindingIdx, paramName };
}

static void InitializeBinding(smd::CommonBindingData& binding, SpirvResourceData resourceData, const ShaderParametersMetaData& parametersMetaData)
{
	binding.AddFlag(parametersMetaData.GetAdditionalBindingFlags(resourceData.setIdx, resourceData.bindingIdx));
}


class SpirvExposedParametersBuilder
{
public:

	SpirvExposedParametersBuilder(Uint32 setIdx,
								  Uint32 bindingIdx,
								  const spirv_cross::Compiler& compiler,
								  rhi::EShaderStage shaderStage,
								  const ShaderParametersMetaData& parametersMetaData,
								  smd::ShaderMetaData& outShaderMetaData)
		: m_setIdx(setIdx)
		, m_bindingIdx(bindingIdx)
		, m_compiler(compiler)
		, m_shaderStage(shaderStage)
		, m_parametersMetaData(parametersMetaData)
		, m_outShaderMetaData(outShaderMetaData)
	{ }

	void AddMembersParameters(const spirv_cross::SPIRType& type) const;

private:

	void AddMemberParameterImpl(const spirv_cross::SPIRType& type, Uint32 memberIdx, Uint32 offset, Uint32 stride) const;

	/** return SPIRType of typeID for single element variables, and SPIRType of single element type for arrays */
	const spirv_cross::SPIRType& GetSingleElementType(spirv_cross::TypeID typeID) const;

	Uint32							m_setIdx;
	Uint32							m_bindingIdx;
	const spirv_cross::Compiler&	m_compiler;
	rhi::EShaderStage				m_shaderStage;
	const ShaderParametersMetaData& m_parametersMetaData;
	smd::ShaderMetaData&			m_outShaderMetaData;
};

void SpirvExposedParametersBuilder::AddMembersParameters(const spirv_cross::SPIRType& type) const
{
	SPT_PROFILER_FUNCTION();

	const Uint32 membersNum = static_cast<Uint32>(type.member_types.size());
	for (Uint32 memberIdx = 0; memberIdx < membersNum; ++memberIdx)
	{
		AddMemberParameterImpl(type, memberIdx, 0, 0);
	}
}

void SpirvExposedParametersBuilder::AddMemberParameterImpl(const spirv_cross::SPIRType& type, Uint32 memberIdx, Uint32 offset, Uint32 stride) const
{
	SPT_PROFILER_FUNCTION();

	const spirv_cross::TypeID variableTypeID		= type.member_types[memberIdx];
	const spirv_cross::SPIRType& singleElementType	= GetSingleElementType(variableTypeID);
	const spirv_cross::TypeID singleElementTypeID	= singleElementType.self;

	const Bool isArray = variableTypeID != singleElementTypeID;
	SPT_CHECK(!isArray || stride == 0); // if stride is not zero, it means that current type is element of an array, and for now we don't support nested arrays

	// global offset in processed binding
	const Uint32 memberOffset			= offset + m_compiler.type_struct_member_offset(type, memberIdx);
	const lib::HashedString memberName	= m_compiler.get_member_name(type.self, memberIdx);

	if (m_parametersMetaData.HasMeta(memberName, meta::exposeInner))
	{
		// send most nested stride to next recursion calls
		const Uint32 elementsStride = isArray ? m_compiler.type_struct_member_array_stride(type, memberIdx) : stride;

		const Uint32 membersNum = static_cast<Uint32>(singleElementType.member_types.size());
		for (Uint32 idx = 0; idx < membersNum; ++idx)
		{
			AddMemberParameterImpl(singleElementType, idx, memberOffset, elementsStride);
		}
	}
	else
	{
		const Uint32 memberSize		= static_cast<Uint32>(m_compiler.get_declared_struct_member_size(type, memberIdx));
		const Uint32 memberStride	= isArray ? m_compiler.type_struct_member_array_stride(type, memberIdx) : 0;

		SPT_CHECK(offset		<= static_cast<Uint32>(std::numeric_limits<Uint16>::max()));
		SPT_CHECK(memberSize	<= static_cast<Uint32>(std::numeric_limits<Uint16>::max()));
		SPT_CHECK(memberStride	<= static_cast<Uint32>(std::numeric_limits<Uint16>::max()));

		const smd::ShaderDataParam dataParam(static_cast<Uint16>(offset), static_cast<Uint16>(memberSize), static_cast<Uint16>(memberStride));
		m_outShaderMetaData.AddShaderDataParam(m_setIdx, m_bindingIdx, memberName, dataParam);
	}
}

const spirv_cross::SPIRType& SpirvExposedParametersBuilder::GetSingleElementType(spirv_cross::TypeID typeID) const
{
	const spirv_cross::SPIRType& type		= m_compiler.get_type(typeID);
	const spirv_cross::TypeID elementTypeID = type.self; // I don't really know why, but it's same as typeID for single variables and for arrays it's id of type of single element of that array

	return elementTypeID == typeID
		? type // single variable
		: m_compiler.get_type(elementTypeID); // array (return type of array element)
}

} // helper

static void AddUniformBuffer(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& uniformBufferResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	const auto [setIdx, bindingIdx, paramName] = helper::GetResourceData(compiler, uniformBufferResource);

	const spirv_cross::SPIRType& bufferType = compiler.get_type(uniformBufferResource.base_type_id);
	const SizeType uniformSize = compiler.get_declared_struct_size_runtime_array(bufferType, 1);

	smd::BufferBindingData uniformBufferBinding;
	uniformBufferBinding.SetSize(static_cast<Uint32>(uniformSize));
	helper::InitializeBinding(uniformBufferBinding, helper::SpirvResourceData{ setIdx, bindingIdx, paramName }, parametersMetaData);

	outShaderMetaData.AddShaderBindingData(setIdx, bindingIdx, uniformBufferBinding);
	outShaderMetaData.AddShaderStageToBinding(setIdx, bindingIdx, shaderStage);
	
	const smd::ShaderBufferParamEntry bufferParam(setIdx, bindingIdx);
	outShaderMetaData.AddShaderParamEntry(paramName, bufferParam);

	if (parametersMetaData.HasMeta(paramName, meta::exposeInner))
	{
		const helper::SpirvExposedParametersBuilder exposedParamsBuilder(setIdx, bindingIdx, compiler, shaderStage, parametersMetaData, outShaderMetaData);
		exposedParamsBuilder.AddMembersParameters(bufferType);
	}
}

static void AddStorageBuffer(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& storageBufferResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	const auto [setIdx, bindingIdx, paramName] = helper::GetResourceData(compiler, storageBufferResource);

	const spirv_cross::SPIRType& bufferType = compiler.get_type(storageBufferResource.base_type_id);
	const SizeType bufferSize = compiler.get_declared_struct_size(bufferType);
	const Bool isUnbound = bufferSize == 0;

	smd::BufferBindingData storageBufferBinding;
	storageBufferBinding.AddFlag(smd::EBindingFlags::Storage);
	if (isUnbound)
	{
		storageBufferBinding.SetUnbound();
	}
	else
	{
		storageBufferBinding.SetSize(static_cast<Uint32>(bufferSize));
	}
	helper::InitializeBinding(storageBufferBinding, helper::SpirvResourceData{ setIdx, bindingIdx, paramName }, parametersMetaData);

	outShaderMetaData.AddShaderBindingData(setIdx, bindingIdx, storageBufferBinding);
	outShaderMetaData.AddShaderStageToBinding(setIdx, bindingIdx, shaderStage);
	
	// Don't create parameters for this buffer if name contains dots
	// If it contains dots it's some kind of additional binding, e.g. counter binding for StructuredBuffer
	if (std::find(std::cbegin(paramName.GetView()), std::cend(paramName.GetView()), '.') == std::cend(paramName.GetView()))
	{
		const smd::ShaderBufferParamEntry bufferParam(setIdx, bindingIdx);
		outShaderMetaData.AddShaderParamEntry(paramName, bufferParam);
	}

	if (parametersMetaData.HasMeta(paramName, meta::exposeInner))
	{
		const helper::SpirvExposedParametersBuilder exposedParamsBuilder(setIdx, bindingIdx, compiler, shaderStage, parametersMetaData, outShaderMetaData);
		exposedParamsBuilder.AddMembersParameters(bufferType);
	}
}

static void AddCombinedTextureSampler(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& combinedTextureSamplerResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	const auto [setIdx, bindingIdx, paramName] = helper::GetResourceData(compiler, combinedTextureSamplerResource);
	
	smd::CombinedTextureSamplerBindingData combinedTextureSamplerBinding;
	helper::InitializeBinding(combinedTextureSamplerBinding, helper::SpirvResourceData{ setIdx, bindingIdx, paramName }, parametersMetaData);

	outShaderMetaData.AddShaderBindingData(setIdx, bindingIdx, combinedTextureSamplerBinding);
	outShaderMetaData.AddShaderStageToBinding(setIdx, bindingIdx, shaderStage);

	const smd::ShaderCombinedTextureSamplerParamEntry paramEntry(setIdx, bindingIdx);
	outShaderMetaData.AddShaderParamEntry(paramName, paramEntry);
}

static void AddStorageTexture(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& textureResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	const auto [setIdx, bindingIdx, paramName] = helper::GetResourceData(compiler, textureResource);
	
	smd::TextureBindingData textureBinding;
	textureBinding.AddFlag(smd::EBindingFlags::Storage);
	helper::InitializeBinding(textureBinding, helper::SpirvResourceData{ setIdx, bindingIdx, paramName }, parametersMetaData);

	outShaderMetaData.AddShaderBindingData(setIdx, bindingIdx, textureBinding);
	outShaderMetaData.AddShaderStageToBinding(setIdx, bindingIdx, shaderStage);

	const smd::ShaderTextureParamEntry paramEntry(setIdx, bindingIdx);
	outShaderMetaData.AddShaderParamEntry(paramName, paramEntry);
}

static void AddSampler(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& samplerResource, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	const auto [setIdx, bindingIdx, paramName] = helper::GetResourceData(compiler, samplerResource);

	smd::SamplerBindingData samplerBinding;
	helper::InitializeBinding(samplerBinding, helper::SpirvResourceData{ setIdx, bindingIdx, paramName }, parametersMetaData);

	outShaderMetaData.AddShaderBindingData(setIdx, bindingIdx, samplerBinding);
	outShaderMetaData.AddShaderStageToBinding(setIdx, bindingIdx, shaderStage);

	if (!samplerBinding.IsImmutable())
	{
		const smd::ShaderSamplerParamEntry paramEntry(setIdx, bindingIdx);
		outShaderMetaData.AddShaderParamEntry(paramName, paramEntry);
	}
}

static void BuildShaderMetaData(const spirv_cross::Compiler& compiler, rhi::EShaderStage shaderStage, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

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
			AddStorageTexture(compiler, texture, shaderStage, parametersMetaData, outShaderMetaData);
		});

	std::for_each(std::cbegin(resources.separate_samplers), std::cend(resources.separate_samplers),
		[&](const spirv_cross::Resource& sampler)
		{
			AddSampler(compiler, sampler, shaderStage, parametersMetaData, outShaderMetaData);
		});

	outShaderMetaData.PostInitialize();
}

} // priv

void ShaderMetaDataBuilder::BuildShaderMetaData(const CompiledShader& shader, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	const spirv_cross::Compiler compiler(shader.GetBinary());
	priv::BuildShaderMetaData(compiler, shader.GetStage(), parametersMetaData, outShaderMetaData);
}

} // spt::sc
