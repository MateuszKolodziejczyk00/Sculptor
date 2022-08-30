#include "Shader.h"
#include "RendererUtils.h"
#include "CurrentFrameContext.h"

namespace spt::renderer
{

Shader::Shader(const RendererResourceName& name, const lib::DynamicArray<rhi::ShaderModuleDefinition>& moduleDefinitions, const lib::SharedPtr<smd::ShaderMetaData>& metaData)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!!metaData);

	m_shaderModules.reserve(moduleDefinitions.size());

	std::transform(std::cbegin(moduleDefinitions), std::cend(moduleDefinitions), std::back_inserter(m_shaderModules),
		[&](const rhi::ShaderModuleDefinition& moduleDef)
		{
			rhi::RHIShaderModule rhiShaderModule;
			rhiShaderModule.InitializeRHI(moduleDef);

#if RENDERER_VALIDATION
			const lib::HashedString moduleName = std::invoke(
				[&](const rhi::EShaderStage stage)
				{
					switch (stage)
					{
					case rhi::EShaderStage::Vertex:		return name.Get().ToString() + "_Vertex";
					case rhi::EShaderStage::Fragment:	return name.Get().ToString() + "_Fragment";
					case rhi::EShaderStage::Compute:	return name.Get().ToString() + "_Compute";

					default:

						SPT_CHECK_NO_ENTRY();
						return lib::String();
					}
				}, moduleDef.stage);
			
			rhiShaderModule.SetName(moduleName);

#endif // RENDERER_VALIDATION

			return rhiShaderModule;
		});
	
	m_metaData = metaData;
	
	m_pipelineType = SelectPipelineType(moduleDefinitions);
}

Shader::~Shader()
{
	CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
	[resources = std::move(m_shaderModules)]() mutable
	{
		std::for_each(std::begin(resources), std::end(resources), std::mem_fn(&rhi::RHIShaderModule::ReleaseRHI));
	});
}

const lib::DynamicArray<rhi::RHIShaderModule>& Shader::GetShaderModules() const
{
	return m_shaderModules;
}

const lib::SharedPtr<smd::ShaderMetaData>& Shader::GetMetaData() const
{
	return m_metaData;
}

rhi::EPipelineType Shader::GetPipelineType() const
{
	return m_pipelineType;
}

rhi::EPipelineType Shader::SelectPipelineType(const lib::DynamicArray<rhi::ShaderModuleDefinition>& moduleDefinitions) const
{
	if (moduleDefinitions.empty())
	{
		return rhi::EPipelineType::None;
	}

	const rhi::EPipelineType pipelineType = rhi::GetPipelineTypeForShaderStage(moduleDefinitions[0].stage);

	const Bool isValidCombinationOfShaders = std::all_of(std::begin(moduleDefinitions) + 1, std::end(moduleDefinitions),
		[pipelineType](const rhi::ShaderModuleDefinition& moduleDefinition)
		{
			return rhi::GetPipelineTypeForShaderStage(moduleDefinition.stage) == pipelineType;
		});

	SPT_CHECK(isValidCombinationOfShaders); // all shaders must belong to same pipeline, e.g. compute + vertex shaders combination is invalid

	return pipelineType;
}

} // spt::renderer

