#include "Common/Compiler/ShaderCompiler.h"
#include "Common/ShaderCompilationInput.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/CompilationErrorsLogger.h"
#include "Common/MetaData/ShaderMetaDataPreprocessor.h"
#include "Common/ShaderFileReader.h"
#include "Utility/String/StringUtils.h"

#include <filesystem>

#define NOMINMAX
#include <windows.h>
#include "dxc/dxcapi.h"

#include "wrl/client.h"
using namespace Microsoft::WRL; // ComPtr

namespace spt::sc
{

SPT_DEFINE_LOG_CATEGORY(ShaderCompiler, true)

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

LPCWSTR GetTargetEnvironment(ETargetEnvironment target) 
{
	switch (target)
	{
	case ETargetEnvironment::Vulkan_1_3:
		return L"-fspv-target-env=vulkan1.3";
	
	default:
		SPT_CHECK_NO_ENTRY();
		return L"";
	}
}

LPCWSTR GetShaderTargetProfile(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::Vertex:			return L"vs_6_4";
	case rhi::EShaderStage::Fragment:		return L"ps_6_4";
	case rhi::EShaderStage::Compute:		return L"cs_6_4";

	default:

		SPT_CHECK_NO_ENTRY();
		return L"";
	}
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// DxcArguments ==================================================================================

class DxcArguments
{
public:

	DxcArguments();

	void Append(const char* name, const char* value = nullptr);
	void Append(const wchar_t* name, const wchar_t* value = nullptr);

	void Append(lib::WString name, lib::WString value = lib::WString());

	SPT_NODISCARD std::pair<const wchar_t**, Uint32> GetArgs();

private:

	lib::DynamicArray<const wchar_t*> m_args;
	lib::DynamicArray<lib::WString> m_argStrings;
};

DxcArguments::DxcArguments()
{ 
	m_argStrings.reserve(32);
}

void DxcArguments::Append(const char* name, const char* value /*= nullptr*/)
{
	lib::WString nameString = lib::StringUtils::ToWideString(lib::String(name));
	lib::WString valueString;

	if (value)
	{
		valueString = lib::StringUtils::ToWideString(lib::String(value));
	}

	Append(std::move(nameString), std::move(valueString));
}

void DxcArguments::Append(const wchar_t* name, const wchar_t* value /*= nullptr*/)
{
	Append(lib::WString(name), value ? lib::WString(value) : lib::WString());
}

void DxcArguments::Append(lib::WString name, lib::WString value /*= lib::WString()*/)
{
	m_argStrings.emplace_back(std::move(name));
	
	if (!value.empty())
	{
		m_argStrings.emplace_back(std::move(value));
	}
}

std::pair<const wchar_t**, Uint32> DxcArguments::GetArgs()
{
	m_args.reserve(m_argStrings.size());
	std::transform(std::cbegin(m_argStrings), std::cend(m_argStrings), std::back_inserter(m_args),
				   [](const lib::WString& arg)
				   {
					   return arg.c_str();
				   });

	return std::make_pair(m_args.data(), static_cast<Uint32>(m_args.size()));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompilerImpl ==================================================================================

class CompilerImpl
{
public:

	CompilerImpl();

	CompiledShader			CompileShader(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings, ShaderParametersMetaData& outParamsMetaData) const;

private:

	ComPtr<IDxcResult>		PreprocessShader(const ShaderSourceCode& sourceCode, const DxcArguments& args) const;
	ComPtr<IDxcResult>		CompileToSPIRV(const ShaderSourceCode& sourceCode, DxcArguments& args) const;

	DxcArguments			BuildArguments(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings) const;

	ComPtr<IDxcUtils>			m_utils;
	ComPtr<IDxcCompiler3>		m_compiler;
	ComPtr<IDxcIncludeHandler>	m_defaultIncludeHandler;
};


CompilerImpl::CompilerImpl()
{
	SPT_PROFILER_FUNCTION();

	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_utils.GetAddressOf()));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_compiler.GetAddressOf()));

	m_utils->CreateDefaultIncludeHandler(m_defaultIncludeHandler.GetAddressOf());
}

CompiledShader CompilerImpl::CompileShader(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings, ShaderParametersMetaData& outParamsMetaData) const
{
	SPT_PROFILER_FUNCTION();

	CompiledShader result{};

	DxcArguments compilerArgs = BuildArguments(shaderPath, sourceCode, compilationSettings);

	const ComPtr<IDxcResult> preprocessResult = PreprocessShader(sourceCode, compilerArgs);

	HRESULT preprocessStatus{};
	preprocessResult->GetStatus(&preprocessStatus);

	if (!SUCCEEDED(preprocessStatus))
	{
		ComPtr<IDxcBlobUtf8> errorsBlob;
		preprocessResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorsBlob.GetAddressOf()), nullptr);
		const lib::String errors(static_cast<const char*>(errorsBlob->GetBufferPointer()), static_cast<SizeType>(errorsBlob->GetStringLength()));
		CompilationErrorsLogger::OutputShaderPreprocessingErrors(shaderPath, sourceCode, errors);
		SPT_LOG_TRACE(ShaderCompiler, "Failed to preprocess shader {0}", shaderPath.c_str());
		return result;
	}

	ComPtr<IDxcBlobUtf8> preprocessedSourceBlob;
	preprocessResult->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(preprocessedSourceBlob.GetAddressOf()), nullptr);
	lib::String preprocessedHLSL(static_cast<const char*>(preprocessedSourceBlob->GetBufferPointer()), static_cast<SizeType>(preprocessedSourceBlob->GetStringLength()));

	ShaderSourceCode preprocessedSourceCode;
	preprocessedSourceCode.SetShaderStage(sourceCode.GetShaderStage());
	preprocessedSourceCode.SetSourceCode(std::move(preprocessedHLSL));

	outParamsMetaData = ShaderMetaDataPrerpocessor::PreprocessShader(preprocessedSourceCode);

	const ComPtr<IDxcResult> compilationResult = CompileToSPIRV(preprocessedSourceCode, compilerArgs);

	HRESULT compilationStatus{};
	compilationResult->GetStatus(&compilationStatus);

	if (!SUCCEEDED(compilationStatus))
	{
		CompilationErrorsLogger::OutputShaderPreprocessedCode(shaderPath, preprocessedSourceCode);

		ComPtr<IDxcBlobUtf8> errorsBlob;
		compilationResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorsBlob.GetAddressOf()), nullptr);
		const lib::String errors(static_cast<const char*>(errorsBlob->GetBufferPointer()), static_cast<SizeType>(errorsBlob->GetStringLength()));
		CompilationErrorsLogger::OutputShaderCompilationErrors(shaderPath, preprocessedSourceCode, errors);
		SPT_LOG_TRACE(ShaderCompiler, "Failed to compile shader {0}", shaderPath.c_str());
		return result;
	}

	ComPtr<IDxcBlob> binaryBlob;
	compilationResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(binaryBlob.GetAddressOf()), nullptr);

	const Uint32* blobPtr = static_cast<const Uint32*>(binaryBlob->GetBufferPointer());
	const SizeType blobSize = binaryBlob->GetBufferSize() / sizeof(Uint32);

	lib::DynamicArray<Uint32> compiledBinary(blobSize);
	memcpy_s(compiledBinary.data(), compiledBinary.size() * sizeof(Uint32), blobPtr, blobSize * sizeof(Uint32));

	result.SetBinary(std::move(compiledBinary));
	result.SetStage(sourceCode.GetShaderStage());

	return result;
}

ComPtr<IDxcResult> CompilerImpl::PreprocessShader(const ShaderSourceCode& sourceCode, const DxcArguments& args) const
{
	SPT_PROFILER_FUNCTION();

	DxcBuffer sourceBuffer{};
	sourceBuffer.Ptr = sourceCode.GetSourceCode().data();
	sourceBuffer.Size = static_cast<Uint32>(sourceCode.GetSourceLength());
	sourceBuffer.Encoding = 0;

	DxcArguments preprocessorArgs = args;
	preprocessorArgs.Append(L"-P");

	const auto [argsPtr, argsNum] = preprocessorArgs.GetArgs();

	ComPtr<IDxcResult> preprocessResult{};
	m_compiler->Compile(&sourceBuffer, argsPtr, argsNum, m_defaultIncludeHandler.Get(), IID_PPV_ARGS(preprocessResult.GetAddressOf()));

	return preprocessResult;
}

ComPtr<IDxcResult> CompilerImpl::CompileToSPIRV(const ShaderSourceCode& sourceCode, DxcArguments& args) const
{
	SPT_PROFILER_FUNCTION();

	DxcBuffer sourceBuffer{};
	sourceBuffer.Ptr = sourceCode.GetSourceCode().data();
	sourceBuffer.Size = static_cast<Uint32>(sourceCode.GetSourceLength());
	sourceBuffer.Encoding = 0;

	const auto [argsPtr, argsNum] = args.GetArgs();

	ComPtr<IDxcResult> compileResult{};
	m_compiler->Compile(&sourceBuffer, argsPtr, argsNum, m_defaultIncludeHandler.Get(), IID_PPV_ARGS(compileResult.GetAddressOf()));

	return compileResult;
}

DxcArguments CompilerImpl::BuildArguments(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings) const
{
	SPT_PROFILER_FUNCTION();

	const lib::String shadersPath = ShaderCompilationEnvironment::GetShadersPath();
	const lib::WString absoluteShadersPath = std::filesystem::absolute(shadersPath);

	const ETargetEnvironment targetEnv = ShaderCompilationEnvironment::GetTargetEnvironment();

	DxcArguments args;
	args.Append(L"-Zpc");
	args.Append(L"-HV", L"2021");
	args.Append(L"-T", priv::GetShaderTargetProfile(sourceCode.GetShaderStage()));
	args.Append(L"-E", L"main");
	args.Append(L"-spirv");
	args.Append(L"-O0");
	args.Append(priv::GetTargetEnvironment(targetEnv));
	args.Append(lib::WString(L"-I"), absoluteShadersPath);

	if (ShaderCompilationEnvironment::ShouldGenerateDebugInfo())
	{
		args.Append(lib::WString(L"-Zi"));
	}

	const lib::DynamicArray<lib::HashedString>& macros = compilationSettings.GetMacros();

	lib::DynamicArray<lib::WString> macrosAsWideString;
	macrosAsWideString.reserve(macros.size());

	std::transform(std::cbegin(macros), std::cend(macros), std::back_inserter(macrosAsWideString),
				   [](const lib::HashedString& macro)
				   {
					   return lib::StringUtils::ToWideString(macro.GetView());
				   });

	for(const lib::WString& macro : macrosAsWideString)
	{
		args.Append(lib::WString(L"-D"), macro.c_str());
	}
	return args;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderCompiler ================================================================================

ShaderCompiler::ShaderCompiler()
{
	SPT_PROFILER_FUNCTION();

	m_impl = std::make_unique<CompilerImpl>();
}

ShaderCompiler::~ShaderCompiler() = default;

CompiledShader ShaderCompiler::CompileShader(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings, ShaderParametersMetaData& outParamsMetaData) const
{
	SPT_PROFILER_FUNCTION();

	return m_impl->CompileShader(shaderPath, sourceCode, compilationSettings, outParamsMetaData);
}

} // spt::sc