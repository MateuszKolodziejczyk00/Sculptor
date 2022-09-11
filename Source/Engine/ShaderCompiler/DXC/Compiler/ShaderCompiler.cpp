#include "Common/Compiler/ShaderCompiler.h"
#include "Common/ShaderCompilationInput.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/CompilationErrorsLogger.h"
#include "Common/MetaData/ShaderMetaDataPreprocessor.h"
#include "Common/ShaderFileReader.h"
#include "Utility/String/StringUtils.h"

#define NOMINMAX
#include <windows.h>
#include "dxc/dxcapi.h"

#include "wrl/client.h"
using namespace Microsoft::WRL; // ComPtr

namespace spt::sc
{

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
// Includer ======================================================================================

class IncludeHandler : public IDxcIncludeHandler
{
public:

	IncludeHandler(ComPtr<IDxcUtils> inUtils, ComPtr<IDxcIncludeHandler> inDefaultIncludeHandler);

	virtual HRESULT STDMETHODCALLTYPE LoadSource(_In_z_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource) override;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override;

	virtual ULONG STDMETHODCALLTYPE AddRef(void) override;
	virtual ULONG STDMETHODCALLTYPE Release(void) override;

private:

	lib::String ReadShaderCode(const lib::String& path) const;

	ComPtr<IDxcUtils>			m_utils;
	ComPtr<IDxcIncludeHandler>	m_defaultIncludeHandler;

	// cache string to make sure that pointers to source won't be destroyed until compilation finishes
	lib::HashMap<lib::String, lib::String> m_includedSourceCodes;
};

IncludeHandler::IncludeHandler(ComPtr<IDxcUtils> inUtils, ComPtr<IDxcIncludeHandler> inDefaultIncludeHandler)
	: m_utils(std::move(inUtils))
	, m_defaultIncludeHandler(std::move(inDefaultIncludeHandler))
{ }

HRESULT STDMETHODCALLTYPE IncludeHandler::LoadSource(_In_z_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource)
{
	SPT_PROFILER_FUNCTION();

	const lib::String filePath = lib::StringUtils::ToMultibyteString(pFilename);

	if (!m_includedSourceCodes.contains(filePath))
	{
		const lib::String& code = m_includedSourceCodes[filePath] = ReadShaderCode(filePath);

		ComPtr<IDxcBlobEncoding> encoding;
		m_utils->CreateBlob(code.data(), static_cast<Uint32>(code.size()), 0, encoding.GetAddressOf());
		*ppIncludeSource = encoding.Detach();
	}

	return S_OK;
}

lib::String IncludeHandler::ReadShaderCode(const lib::String& path) const
{
	SPT_PROFILER_FUNCTION();

	return ShaderFileReader::ReadShaderFileAbsolute(path.c_str());
}

HRESULT STDMETHODCALLTYPE IncludeHandler::QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject)
{
	return m_defaultIncludeHandler->QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE IncludeHandler::AddRef(void)
{
	return 0;
}

ULONG STDMETHODCALLTYPE IncludeHandler::Release(void)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompilerImpl ==================================================================================

class CompilerImpl
{
public:

	CompilerImpl();

	CompiledShader			CompileShader(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings, ShaderParametersMetaData& outParamsMetaData) const;

private:

	ComPtr<IDxcResult>		PreprocessShader(const ShaderSourceCode& sourceCode) const;
	ComPtr<IDxcResult>		CompileToSPIRV(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings) const;

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

	const ComPtr<IDxcResult> preprocessResult = PreprocessShader(sourceCode);

	HRESULT preprocessStatus{};
	preprocessResult->GetStatus(&preprocessStatus);

	if (!SUCCEEDED(preprocessStatus))
	{
		ComPtr<IDxcBlobUtf8> errorsBlob;
		preprocessResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorsBlob.GetAddressOf()), nullptr);
		const lib::String errors(static_cast<const char*>(errorsBlob->GetBufferPointer()), static_cast<SizeType>(errorsBlob->GetStringLength()));
		CompilationErrorsLogger::OutputShaderPreprocessingErrors(shaderPath, sourceCode, errors);
		return result;
	}

	ComPtr<IDxcBlobUtf8> preprocessedSourceBlob;
	preprocessResult->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(preprocessedSourceBlob.GetAddressOf()), nullptr);
	lib::String preprocessedHLSL(static_cast<const char*>(preprocessedSourceBlob->GetBufferPointer()), static_cast<SizeType>(preprocessedSourceBlob->GetStringLength()));

	ShaderSourceCode preprocessedSourceCode;
	preprocessedSourceCode.SetShaderStage(sourceCode.GetShaderStage());
	preprocessedSourceCode.SetSourceCode(std::move(preprocessedHLSL));

	outParamsMetaData = ShaderMetaDataPrerpocessor::PreprocessShaderParametersMetaData(preprocessedSourceCode);

	const ComPtr<IDxcResult> compilationResult = CompileToSPIRV(preprocessedSourceCode, compilationSettings);

	HRESULT compilationStatus{};
	compilationResult->GetStatus(&compilationStatus);

	if (!SUCCEEDED(compilationStatus))
	{
		CompilationErrorsLogger::OutputShaderPreprocessedCode(shaderPath, preprocessedSourceCode);

		ComPtr<IDxcBlobUtf8> errorsBlob;
		compilationResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorsBlob.GetAddressOf()), nullptr);
		const lib::String errors(static_cast<const char*>(errorsBlob->GetBufferPointer()), static_cast<SizeType>(errorsBlob->GetStringLength()));
		CompilationErrorsLogger::OutputShaderCompilationErrors(shaderPath, preprocessedSourceCode, errors);
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

ComPtr<IDxcResult> CompilerImpl::PreprocessShader(const ShaderSourceCode& sourceCode) const
{
	SPT_PROFILER_FUNCTION();

	DxcBuffer sourceBuffer{};
	sourceBuffer.Ptr = sourceCode.GetSourceCode().data();
	sourceBuffer.Size = static_cast<Uint32>(sourceCode.GetSourceLength());
	sourceBuffer.Encoding = 0;

	LPCWSTR preprocessArgs[] = 
	{
		L"-P",
		L"main"
	};

	ComPtr<IDxcResult> preprocessResult{};
	m_compiler->Compile(&sourceBuffer, preprocessArgs, SPT_ARRAY_SIZE(preprocessArgs), nullptr, IID_PPV_ARGS(preprocessResult.GetAddressOf()));

	return preprocessResult;
}

ComPtr<IDxcResult> CompilerImpl::CompileToSPIRV(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings) const
{
	SPT_PROFILER_FUNCTION();

	DxcBuffer sourceBuffer{};
	sourceBuffer.Ptr = sourceCode.GetSourceCode().data();
	sourceBuffer.Size = static_cast<Uint32>(sourceCode.GetSourceLength());
	sourceBuffer.Encoding = 0;

	const ETargetEnvironment targetEnv = ShaderCompilationEnvironment::GetTargetEnvironment();
	SPT_CHECK(targetEnv != ETargetEnvironment::None);

	lib::DynamicArray<LPCWSTR> compilationArgs =
	{
		L"-Zpc",
		L"-HV",
		L"2021",
		L"-T",
		priv::GetShaderTargetProfile(sourceCode.GetShaderStage()),
		L"-E",
		L"main",
		L"-spirv",
		priv::GetTargetEnvironment(targetEnv),
		L"-fspv-reflect"
	};

	if (ShaderCompilationEnvironment::ShouldGenerateDebugInfo())
	{
		compilationArgs.emplace_back(L"-Qstrip_debug");
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
		compilationArgs.emplace_back(L"-D");
		compilationArgs.emplace_back(macro.data());
	}

	IncludeHandler includeHandler(m_utils, m_defaultIncludeHandler);

	ComPtr<IDxcResult> compileResult{};
	m_compiler->Compile(&sourceBuffer, compilationArgs.data(), static_cast<Uint32>(compilationArgs.size()), &includeHandler, IID_PPV_ARGS(compileResult.GetAddressOf()));

	return compileResult;
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