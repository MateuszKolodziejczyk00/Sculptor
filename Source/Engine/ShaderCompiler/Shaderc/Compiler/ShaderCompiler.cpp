#include "Common/Compiler/ShaderCompiler.h"
#include "Common/ShaderCompilationInput.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/CompilationErrorsLogger.h"

#include <fstream>

#include "shaderc/shaderc.hpp"

namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

Bool GetTargetEnvironment(ETargetEnvironment target, shaderc_target_env& outTargetEnv, Uint32& outVersion)
{
	Bool success = true;

	switch (target)
	{
	case ETargetEnvironment::Vulkan_1_3:
		outTargetEnv	= shaderc_target_env::shaderc_target_env_vulkan;
		outVersion		= shaderc_env_version_vulkan_1_3;

		break;

	default:
		success = false;

		break;
	}

	return success;
}

shaderc_shader_kind GetShaderKind(ECompiledShaderType type)
{
	switch (type)
	{
	case spt::sc::ECompiledShaderType::Vertex:			return shaderc_vertex_shader;
	case spt::sc::ECompiledShaderType::Fragment:		return shaderc_fragment_shader;
	case spt::sc::ECompiledShaderType::Compute:			return shaderc_compute_shader;
	default:

		SPT_CHECK_NO_ENTRY();
		return shaderc_vertex_shader;
	}
}

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Includer ======================================================================================

class Includer : public shaderc::CompileOptions::IncluderInterface
{
	struct IncludeData
	{
		lib::String name;
		lib::String code;
	};

public:

	Includer() = default;

	void							SetCurrentShaderLocation(lib::String path);

	// Begin shaderc::CompileOptions::IncluderInterface overrides
	virtual shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override;
	virtual void					ReleaseInclude(shaderc_include_result* data) override;
	// End shaderc::CompileOptions::IncluderInterface overrides

private:

	lib::String						GetIncludedFilePath(const lib::String& name, shaderc_include_type includeType) const;

	lib::String						ReadShaderCode(const lib::String& path) const;

	IncludeData*					CreateIncludeData(lib::String name, lib::String code);
	void							DestroyIncludeData(IncludeData* data);

	lib::String						m_path;
};


void Includer::SetCurrentShaderLocation(lib::String path)
{
	m_path = std::move(path);
}

shaderc_include_result* Includer::GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth)
{
	SPT_PROFILE_FUNCTION();

	lib::String fileName = requesting_source;

	const lib::String filePath = GetIncludedFilePath(fileName, type);

	lib::String code = ReadShaderCode(filePath);

	IncludeData* includeData = CreateIncludeData(std::move(fileName), std::move(code));

	shaderc_include_result* result = new shaderc_include_result();
	result->content				= includeData->code.data();
	result->content_length		= includeData->code.size();
	result->source_name			= includeData->name.data();
	result->source_name_length	= includeData->name.size();
	result->user_data			= includeData;

	return result;
}

void Includer::ReleaseInclude(shaderc_include_result* data)
{
	DestroyIncludeData(static_cast<IncludeData*>(data->user_data));
	delete data;
}

lib::String Includer::GetIncludedFilePath(const lib::String& name, shaderc_include_type includeType) const
{
	if (includeType == shaderc_include_type_relative)
	{
		return m_path + "/" + name;
	}
	else
	{
		return ShaderCompilationEnvironment::GetShadersPath() + "/" + name;
	}
}

lib::String Includer::ReadShaderCode(const lib::String& path) const
{
	SPT_PROFILE_FUNCTION();

	std::ifstream file(path.c_str());

	lib::String code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return code;
}

Includer::IncludeData* Includer::CreateIncludeData(lib::String name, lib::String code)
{
	IncludeData* data = new IncludeData();
	data->name = std::move(name);
	data->code = std::move(code);
	return data;
}

void Includer::DestroyIncludeData(IncludeData* data)
{
	delete data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompilerImpl ==================================================================================

class CompilerImpl
{
public:

	CompilerImpl();

	ShaderCompilationResult		CompileShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

protected:

	shaderc::CompileOptions		CreateCompileOptions(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings) const;

	shaderc::PreprocessedSourceCompilationResult	PreprocessShader(const ShaderSourceCode& sourceCode, const shaderc::CompileOptions& options) const;

	shaderc::SpvCompilationResult					CompileToSpirv(const ShaderSourceCode& preprocessedSourceCode, const shaderc::CompileOptions& options) const;

private:

	shaderc::Compiler			m_compiler;
};


CompilerImpl::CompilerImpl()
{ }

ShaderCompilationResult CompilerImpl::CompileShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(m_compiler.IsValid());

	ShaderCompilationResult output;

	shaderc::CompileOptions options = CreateCompileOptions(sourceCode, compilationSettings);

	const shaderc::PreprocessedSourceCompilationResult preprocessResult = PreprocessShader(sourceCode, options);

	if (preprocessResult.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		CompilationErrorsLogger::OutputShaderPreprocessingErrors(sourceCode, preprocessResult.GetErrorMessage());
		return output;
	}

	ShaderSourceCode preprocessedShaderSource(sourceCode.GetName());
	preprocessedShaderSource.SetShaderType(sourceCode.GetType());
	preprocessedShaderSource.SetSourceCode(lib::String(preprocessResult.cbegin(), preprocessResult.cend()));

	const shaderc::CompilationResult compilationResult = CompileToSpirv(preprocessedShaderSource, options);

	if (compilationResult.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		CompilationErrorsLogger::OutputShaderPreprocessedCode(preprocessedShaderSource);
		CompilationErrorsLogger::OutputShaderCompilationErrors(preprocessedShaderSource, compilationResult.GetErrorMessage());
		return output;
	}

	output.m_binary = lib::DynamicArray<Uint32>(compilationResult.cbegin(), compilationResult.cend());

	return output;
}

shaderc::CompileOptions CompilerImpl::CreateCompileOptions(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings) const
{
	SPT_PROFILE_FUNCTION();

	shaderc::CompileOptions options;

	const lib::DynamicArray<lib::HashedString>& macros = compilationSettings.GetMacros();

	for (const lib::HashedString& macro: macros)
	{
		options.AddMacroDefinition(macro.ToString());
	}

	const ETargetEnvironment targetEnv = ShaderCompilationEnvironment::GetTargetEnvironment();
	SPT_CHECK(targetEnv != ETargetEnvironment::None);

	shaderc_target_env shadercTargetEnv = shaderc_target_env_default;
	Uint32 shadercVersion = idxNone<Uint32>;

	const Bool success = priv::GetTargetEnvironment(targetEnv, shadercTargetEnv, shadercVersion);
	SPT_CHECK(success);

	options.SetTargetEnvironment(shadercTargetEnv, shadercVersion);
	
	if (ShaderCompilationEnvironment::ShouldGenerateDebugInfo())
	{
		options.SetGenerateDebugInfo();
	}

	options.SetIncluder(std::make_unique<Includer>());

	return options;
}

shaderc::PreprocessedSourceCompilationResult CompilerImpl::PreprocessShader(const ShaderSourceCode& sourceCode, const shaderc::CompileOptions& options) const
{
	SPT_PROFILE_FUNCTION();

	const shaderc_shader_kind shaderKind = priv::GetShaderKind(sourceCode.GetType());

	shaderc::PreprocessedSourceCompilationResult presprocessResult = m_compiler.PreprocessGlsl(	sourceCode.GetSourcePtr(),
																								sourceCode.GetSourceLength(),
																								shaderKind,
																								sourceCode.GetName().GetData(),
																								options);

	return presprocessResult;
}

shaderc::SpvCompilationResult CompilerImpl::CompileToSpirv(const ShaderSourceCode& preprocessedSourceCode, const shaderc::CompileOptions& options) const
{
	SPT_PROFILE_FUNCTION();

	const shaderc_shader_kind shaderKind = priv::GetShaderKind(preprocessedSourceCode.GetType());

	shaderc::SpvCompilationResult result = m_compiler.CompileGlslToSpv(preprocessedSourceCode.GetSourceCode(), shaderKind, preprocessedSourceCode.GetName().GetData());

	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderCompiler ================================================================================

ShaderCompiler::ShaderCompiler()
{ }

ShaderCompilationResult ShaderCompiler::CompileShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings)
{
	return m_impl->CompileShader(sourceCode, compilationSettings);
}

}
