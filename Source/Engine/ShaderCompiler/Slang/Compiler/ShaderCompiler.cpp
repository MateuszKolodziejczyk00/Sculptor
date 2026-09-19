#include "Common/Compiler/ShaderCompiler.h"
#include "Common/ShaderCompilationInput.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/CompilationErrorsLogger.h"
#include "Common/MetaData/ShaderMetaDataPreprocessor.h"
#include "Utility/String/StringUtils.h"

#include <filesystem>

#define NOMINMAX
#include <windows.h>
#include "dxc/dxcapi.h"

#include "wrl/client.h"
using namespace Microsoft::WRL; // ComPtr

#include "slang.h"
#include "slang-com-ptr.h"

namespace spt::sc
{

SPT_DEFINE_LOG_CATEGORY(ShaderCompiler, true)

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

SlangStage GetSlangShaderStage(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::Vertex:			return SLANG_STAGE_VERTEX;
	case rhi::EShaderStage::Task:			return SLANG_STAGE_AMPLIFICATION;
	case rhi::EShaderStage::Mesh:			return SLANG_STAGE_MESH;
	case rhi::EShaderStage::Fragment:		return SLANG_STAGE_FRAGMENT;
	case rhi::EShaderStage::Compute:		return SLANG_STAGE_COMPUTE;
	case rhi::EShaderStage::RTGeneration:	return SLANG_STAGE_RAY_GENERATION;
	case rhi::EShaderStage::RTAnyHit:		return SLANG_STAGE_ANY_HIT;
	case rhi::EShaderStage::RTClosestHit:	return SLANG_STAGE_CLOSEST_HIT;
	case rhi::EShaderStage::RTMiss:			return SLANG_STAGE_MISS;
	case rhi::EShaderStage::RTIntersection:	return SLANG_STAGE_INTERSECTION;

	default:
		SPT_CHECK_NO_ENTRY();
		return SLANG_STAGE_NONE;
	}
}

lib::String GetShaderStageMacro(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::Vertex:			return "SPT_VERTEX_SHADER";
	case rhi::EShaderStage::Task:			return "SPT_TASK_SHADER";
	case rhi::EShaderStage::Mesh:			return "SPT_MESH_SHADER";
	case rhi::EShaderStage::Fragment:		return "SPT_FRAGMENT_SHADER";
	case rhi::EShaderStage::Compute:		return "SPT_COMPUTE_SHADER";
	case rhi::EShaderStage::RTGeneration:	return "SPT_RT_GENERATION_SHADER";
	case rhi::EShaderStage::RTAnyHit:		return "SPT_RT_ANY_HIT_SHADER";
	case rhi::EShaderStage::RTClosestHit:	return "SPT_RT_CLOSEST_HIT_SHADER";
	case rhi::EShaderStage::RTMiss:			return "SPT_RT_MISS_SHADER";
	case rhi::EShaderStage::RTIntersection:	return "SPT_RT_INTERSECTION_SHADER";

	default:
		SPT_CHECK_NO_ENTRY();
		return lib::String();
	}
}

const wchar_t* GetDxcShaderTargetProfile(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::Vertex:			return L"vs_6_6";
	case rhi::EShaderStage::Task:			return L"as_6_6";
	case rhi::EShaderStage::Mesh:			return L"ms_6_6";
	case rhi::EShaderStage::Fragment:		return L"ps_6_6";
	case rhi::EShaderStage::Compute:		return L"cs_6_6";
	case rhi::EShaderStage::RTGeneration:
	case rhi::EShaderStage::RTAnyHit:
	case rhi::EShaderStage::RTClosestHit:
	case rhi::EShaderStage::RTMiss:
	case rhi::EShaderStage::RTIntersection:return L"lib_6_6";

	default:
		SPT_CHECK_NO_ENTRY();
		return L"";
	}
}

lib::String BlobToString(slang::IBlob* blob)
{
	if (!blob || blob->getBufferSize() == 0)
	{
		return lib::String();
	}

	const char* const data = static_cast<const char*>(blob->getBufferPointer());
	SizeType size = static_cast<SizeType>(blob->getBufferSize());
	if (data[size - 1] == '\0')
	{
		--size;
	}

	return lib::String(data, size);
}

slang::CompilerOptionEntry MakeIntOption(slang::CompilerOptionName name, int32_t value)
{
	slang::CompilerOptionEntry entry{};
	entry.name = name;
	entry.value.kind = slang::CompilerOptionValueKind::Int;
	entry.value.intValue0 = value;
	return entry;
}

slang::CompilerOptionEntry MakeStringOption(slang::CompilerOptionName name, const char* value)
{
	slang::CompilerOptionEntry entry{};
	entry.name = name;
	entry.value.kind = slang::CompilerOptionValueKind::String;
	entry.value.stringValue0 = value;
	return entry;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// DxcArguments ==================================================================================

class DxcArguments
{
public:

	void Append(lib::WString argument)
	{
		m_arguments.emplace_back(std::move(argument));
	}

	void Append(lib::WString name, lib::WString value)
	{
		m_arguments.emplace_back(std::move(name));
		m_arguments.emplace_back(std::move(value));
	}

	std::pair<const wchar_t**, Uint32> GetArgs()
	{
		m_argumentPointers.clear();
		m_argumentPointers.reserve(m_arguments.size());

		for (const lib::WString& argument : m_arguments)
		{
			m_argumentPointers.emplace_back(argument.c_str());
		}

		return { m_argumentPointers.data(), static_cast<Uint32>(m_argumentPointers.size()) };
	}

private:

	lib::DynamicArray<lib::WString>     m_arguments;
	lib::DynamicArray<const wchar_t*>   m_argumentPointers;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompilerImpl ==================================================================================

class CompilerImpl
{
public:

	CompilerImpl();

	CompiledShader CompileShader(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const ShaderCompilationSettings& compilationSettings, ShaderCompilationMetaData& outCompilationMetaData) const;

private:

	DxcArguments BuildPreprocessorArguments(const lib::String& shaderPath, const ShaderStageCompilationDef& stageCompilationDef, const ShaderCompilationSettings& compilationSettings) const;
	lib::String   PreprocessShader(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const DxcArguments& arguments) const;
	void          PreprocessAdditionalCompilerArgs(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, INOUT DxcArguments& arguments, OUT Bool& outGenerateDebugInfo) const;

	CompiledShader CompileHLSLToSPIRV(const lib::String& shaderPath, const lib::String& preprocessedHLSL, const ShaderStageCompilationDef& stageCompilationDef, Bool generateDebugInfo) const;

	slang::IGlobalSession* GetGlobalSession() const;

	ComPtr<IDxcUtils>                     m_dxcUtils;
	ComPtr<IDxcCompiler3>                 m_dxcCompiler;
	ComPtr<IDxcIncludeHandler>            m_dxcIncludeHandler;
};

CompilerImpl::CompilerImpl()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(SUCCEEDED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_dxcUtils.GetAddressOf()))));
	SPT_CHECK(SUCCEEDED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_dxcCompiler.GetAddressOf()))));
	SPT_CHECK(SUCCEEDED(m_dxcUtils->CreateDefaultIncludeHandler(m_dxcIncludeHandler.GetAddressOf())));
}

slang::IGlobalSession* CompilerImpl::GetGlobalSession() const
{
	static thread_local Slang::ComPtr<slang::IGlobalSession> globalSession;
	if (!globalSession)
	{
		const SlangResult result = slang::createGlobalSession(globalSession.writeRef());
		if (SLANG_FAILED(result) || !globalSession)
		{
			SPT_LOG_TRACE(ShaderCompiler, "Failed to create Slang global session. Error: {}", slang::getLastInternalErrorMessage());
			return nullptr;
		}
	}

	return globalSession;
}

CompiledShader CompilerImpl::CompileShader(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const ShaderCompilationSettings& compilationSettings, ShaderCompilationMetaData& outCompilationMetaData) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(ShaderCompilationEnvironment::GetTargetEnvironment() == ETargetEnvironment::Vulkan_1_3);

	CompiledShader result{};

	Bool generateDebugInfo = ShaderCompilationEnvironment::ShouldGenerateDebugInfo();
	DxcArguments preprocessorArguments = BuildPreprocessorArguments(shaderPath, stageCompilationDef, compilationSettings);
	PreprocessAdditionalCompilerArgs(shaderPath, sourceCode, stageCompilationDef, INOUT preprocessorArguments, OUT generateDebugInfo);
	// Add absolute path as last argument. this will change main file in preprocessed code
	preprocessorArguments.Append(lib::StringUtils::ToWideString(shaderPath));

	lib::String preprocessedHLSL = PreprocessShader(shaderPath, sourceCode, stageCompilationDef, preprocessorArguments);
	if (preprocessedHLSL.empty())
	{
		return result;
	}

	outCompilationMetaData = ShaderMetaDataPrerpocessor::PreprocessShader(preprocessedHLSL);

	// preprocess again to expand descriptor set macros
	preprocessedHLSL = PreprocessShader(shaderPath, preprocessedHLSL, stageCompilationDef, preprocessorArguments);
	if (preprocessedHLSL.empty())
	{
		return result;
	}

	return CompileHLSLToSPIRV(shaderPath, preprocessedHLSL, stageCompilationDef, generateDebugInfo);
}

CompiledShader CompilerImpl::CompileHLSLToSPIRV(const lib::String& shaderPath, const lib::String& preprocessedHLSL, const ShaderStageCompilationDef& stageCompilationDef, Bool generateDebugInfo) const
{
	SPT_PROFILER_FUNCTION();

	CompiledShader result{};

	slang::IGlobalSession* globalSession = GetGlobalSession();
	if (!globalSession)
	{
		return result;
	}

	const SlangCapabilityID descriptorHeapCapability = globalSession->findCapability("spvDescriptorHeapEXT");

	lib::DynamicArray<slang::CompilerOptionEntry> sessionOptions;
	sessionOptions.emplace_back(priv::MakeStringOption(slang::CompilerOptionName::Language, "hlsl"));


	lib::DynamicArray<slang::CompilerOptionEntry> targetOptions;
	targetOptions.emplace_back(priv::MakeIntOption(slang::CompilerOptionName::GLSLForceScalarLayout, 1));
	if (descriptorHeapCapability != 0)
	{
		targetOptions.emplace_back(priv::MakeIntOption(slang::CompilerOptionName::Capability, static_cast<int32_t>(descriptorHeapCapability)));
	}

	if (generateDebugInfo)
	{
		targetOptions.emplace_back(priv::MakeIntOption(slang::CompilerOptionName::DebugInformation, SLANG_DEBUG_INFO_LEVEL_MAXIMAL));
		targetOptions.emplace_back(priv::MakeIntOption(slang::CompilerOptionName::Optimization, SLANG_OPTIMIZATION_LEVEL_NONE));
	}
	else
	{
		targetOptions.emplace_back(priv::MakeIntOption(slang::CompilerOptionName::DebugInformation, SLANG_DEBUG_INFO_LEVEL_STANDARD));
		targetOptions.emplace_back(priv::MakeIntOption(slang::CompilerOptionName::Optimization, SLANG_OPTIMIZATION_LEVEL_MAXIMAL));
	}

	slang::TargetDesc targetDesc{};
	targetDesc.format                      = SLANG_SPIRV;
	targetDesc.profile                     = globalSession->findProfile("sm_6_6");
	targetDesc.forceGLSLScalarBufferLayout = true;
	targetDesc.compilerOptionEntries       = targetOptions.data();
	targetDesc.compilerOptionEntryCount    = static_cast<uint32_t>(targetOptions.size());

	slang::SessionDesc sessionDesc{};
	sessionDesc.targets                  = &targetDesc;
	sessionDesc.targetCount              = 1;
	sessionDesc.compilerOptionEntries    = sessionOptions.data();
	sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(sessionOptions.size());
	sessionDesc.defaultMatrixLayoutMode  = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;


	Slang::ComPtr<slang::ISession> session;
	if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef())) || !session)
	{
		SPT_LOG_TRACE(ShaderCompiler, "Failed to create Slang session for shader {}", shaderPath.c_str());
		return result;
	}

	Slang::ComPtr<slang::IBlob> loadDiagnostics;
	slang::IModule* const slangModule = session->loadModuleFromSourceString("Shader",
	                                                                        shaderPath.c_str(),
	                                                                        preprocessedHLSL.c_str(),
	                                                                        loadDiagnostics.writeRef());
	if (!slangModule)
	{
		const lib::String errors = priv::BlobToString(loadDiagnostics);
		CompilationErrorsLogger::OutputShaderPreprocessedCode(shaderPath, preprocessedHLSL, stageCompilationDef);
		CompilationErrorsLogger::OutputShaderCompilationErrors(shaderPath, preprocessedHLSL, stageCompilationDef, errors);
		SPT_LOG_TRACE(ShaderCompiler, "Failed to compile shader {}\nErrors:\n{}", shaderPath.c_str(), errors.c_str());
		return result;
	}

	const lib::String entryPointName = stageCompilationDef.entryPoint.ToString();

	Slang::ComPtr<slang::IEntryPoint> entryPoint;
	Slang::ComPtr<slang::IBlob> entryPointDiagnostics;
	if (SLANG_FAILED(slangModule->findAndCheckEntryPoint(entryPointName.c_str(),
	                                                     priv::GetSlangShaderStage(stageCompilationDef.stage),
	                                                     entryPoint.writeRef(),
	                                                     entryPointDiagnostics.writeRef()))
		|| !entryPoint)
	{
		const lib::String errors = priv::BlobToString(entryPointDiagnostics);
		CompilationErrorsLogger::OutputShaderPreprocessedCode(shaderPath, preprocessedHLSL, stageCompilationDef);
		CompilationErrorsLogger::OutputShaderCompilationErrors(shaderPath, preprocessedHLSL, stageCompilationDef, errors);
		SPT_LOG_TRACE(ShaderCompiler, "Failed to compile shader {}\nErrors:\n{}", shaderPath.c_str(), errors.c_str());
		return result;
	}

	slang::IComponentType* componentTypes[] = { slangModule, entryPoint };

	Slang::ComPtr<slang::IComponentType> composedProgram;
	Slang::ComPtr<slang::IBlob> composeDiagnostics;
	if (SLANG_FAILED(session->createCompositeComponentType(componentTypes,
	                                                       2,
	                                                       composedProgram.writeRef(),
	                                                       composeDiagnostics.writeRef()))
		|| !composedProgram)
	{
		const lib::String errors = priv::BlobToString(composeDiagnostics);
		CompilationErrorsLogger::OutputShaderPreprocessedCode(shaderPath, preprocessedHLSL, stageCompilationDef);
		CompilationErrorsLogger::OutputShaderCompilationErrors(shaderPath, preprocessedHLSL, stageCompilationDef, errors);
		SPT_LOG_TRACE(ShaderCompiler, "Failed to compile shader {}\nErrors:\n{}", shaderPath.c_str(), errors.c_str());
		return result;
	}

	Slang::ComPtr<slang::IComponentType> linkedProgram;
	Slang::ComPtr<slang::IBlob> linkDiagnostics;
	if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), linkDiagnostics.writeRef())) || !linkedProgram)
	{
		const lib::String errors = priv::BlobToString(linkDiagnostics);
		CompilationErrorsLogger::OutputShaderPreprocessedCode(shaderPath, preprocessedHLSL, stageCompilationDef);
		CompilationErrorsLogger::OutputShaderCompilationErrors(shaderPath, preprocessedHLSL, stageCompilationDef, errors);
		SPT_LOG_TRACE(ShaderCompiler, "Failed to compile shader {}\nErrors:\n{}", shaderPath.c_str(), errors.c_str());
		return result;
	}

	Slang::ComPtr<slang::IBlob> spirvBlob;
	Slang::ComPtr<slang::IBlob> codegenDiagnostics;
	if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, spirvBlob.writeRef(), codegenDiagnostics.writeRef())) || !spirvBlob || spirvBlob->getBufferSize() == 0)
	{
		const lib::String errors = priv::BlobToString(codegenDiagnostics);
		CompilationErrorsLogger::OutputShaderPreprocessedCode(shaderPath, preprocessedHLSL, stageCompilationDef);
		CompilationErrorsLogger::OutputShaderCompilationErrors(shaderPath, preprocessedHLSL, stageCompilationDef, errors);
		SPT_LOG_TRACE(ShaderCompiler, "Failed to compile shader {}\nErrors:\n{}", shaderPath.c_str(), errors.c_str());
		return result;
	}

	const Byte* const spirvBegin = static_cast<const Byte*>(spirvBlob->getBufferPointer());
	result.binary.assign(spirvBegin, spirvBegin + spirvBlob->getBufferSize());
	result.stage = stageCompilationDef.stage;
	result.entryPoint = stageCompilationDef.entryPoint;

	return result;
}

DxcArguments CompilerImpl::BuildPreprocessorArguments(const lib::String& shaderPath, const ShaderStageCompilationDef& stageCompilationDef, const ShaderCompilationSettings& compilationSettings) const
{
	SPT_PROFILER_FUNCTION();

	const lib::String shadersPath = ShaderCompilationEnvironment::GetShadersPath().generic_string();
	const lib::WString absoluteShadersPath = std::filesystem::absolute(shadersPath);

	DxcArguments arguments;
	arguments.Append(L"-Zpc");
	arguments.Append(L"-HV", L"2021");
	arguments.Append(L"-T", priv::GetDxcShaderTargetProfile(stageCompilationDef.stage));
	arguments.Append(L"-E", lib::StringUtils::ToWideString(stageCompilationDef.entryPoint.GetView()));
	arguments.Append(L"-spirv");
	arguments.Append(L"-WX");
	arguments.Append(L"-enable-16bit-types");
	arguments.Append(L"-fvk-use-scalar-layout");
	arguments.Append(L"-fspv-target-env=universal1.5");
	arguments.Append(lib::WString(L"-I"), absoluteShadersPath);
	arguments.Append(lib::WString(L"-I"), absoluteShadersPath + L"/Sculptor");

	for (const lib::HashedString& macro : compilationSettings.GetMacros())
	{
		arguments.Append(L"-D", lib::StringUtils::ToWideString(macro.GetView()));
	}

	arguments.Append(L"-D", lib::StringUtils::ToWideString(priv::GetShaderStageMacro(stageCompilationDef.stage)));
	arguments.Append(L"-D", ShaderCompilationEnvironment::ShouldCompileWithDebugs() ? L"WITH_DEBUGS=1" : L"WITH_DEBUGS=0");
	arguments.Append(std::filesystem::absolute(shaderPath).generic_wstring());

	return arguments;
}

lib::String CompilerImpl::PreprocessShader(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const DxcArguments& arguments) const
{
	SPT_PROFILER_FUNCTION();

	DxcBuffer sourceBuffer{};
	sourceBuffer.Ptr      = sourceCode.data();
	sourceBuffer.Size     = sourceCode.size();
	sourceBuffer.Encoding = 0;

	DxcArguments preprocessorArguments = arguments;
	preprocessorArguments.Append(L"-P", L"Preprocessed");

	const auto [argumentsPtr, argumentsNum] = preprocessorArguments.GetArgs();
	ComPtr<IDxcResult> result;
	SPT_CHECK(SUCCEEDED(m_dxcCompiler->Compile(&sourceBuffer, argumentsPtr, argumentsNum, m_dxcIncludeHandler.Get(), IID_PPV_ARGS(result.GetAddressOf()))));

	HRESULT status{};
	result->GetStatus(&status);
	if (FAILED(status))
	{
		ComPtr<IDxcBlobUtf8> errorsBlob;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorsBlob.GetAddressOf()), nullptr);
		const lib::String errors(static_cast<const char*>(errorsBlob->GetBufferPointer()), static_cast<SizeType>(errorsBlob->GetStringLength()));
		CompilationErrorsLogger::OutputShaderPreprocessingErrors(shaderPath, sourceCode, stageCompilationDef, errors);
		SPT_LOG_TRACE(ShaderCompiler, "Failed to preprocess shader {}\nErrors:\n{}", shaderPath.c_str(), errors.c_str());
		return lib::String();
	}

	ComPtr<IDxcBlobUtf8> preprocessedSourceBlob;
	result->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(preprocessedSourceBlob.GetAddressOf()), nullptr);
	return lib::String(static_cast<const char*>(preprocessedSourceBlob->GetBufferPointer()), static_cast<SizeType>(preprocessedSourceBlob->GetStringLength()));
}

void CompilerImpl::PreprocessAdditionalCompilerArgs(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, INOUT DxcArguments& arguments, OUT Bool& outGenerateDebugInfo) const
{
	SPT_PROFILER_FUNCTION();

	const auto applyMetaData = [&arguments](const ShaderPreprocessingMetaData& metaData)
	{
		for (const lib::HashedString& macro : metaData.macroDefinitions)
		{
			arguments.Append(L"-D", lib::StringUtils::ToWideString(macro.GetView()));
		}
	};

	const ShaderPreprocessingMetaData mainShaderMetaData = ShaderMetaDataPrerpocessor::PreprocessMainShaderFile(sourceCode);
	applyMetaData(mainShaderMetaData);
	outGenerateDebugInfo |= mainShaderMetaData.forceDebugMode;

	const lib::String preprocessedSource = PreprocessShader(shaderPath, sourceCode, stageCompilationDef, arguments);
	if (preprocessedSource.empty())
	{
		return;
	}

	applyMetaData(ShaderMetaDataPrerpocessor::PreprocessAdditionalCompilerArgs(preprocessedSource));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderCompiler ================================================================================

ShaderCompiler::ShaderCompiler()
{
	SPT_PROFILER_FUNCTION();

	m_impl = std::make_unique<CompilerImpl>();
}

ShaderCompiler::~ShaderCompiler() = default;

CompiledShader ShaderCompiler::CompileShader(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const ShaderCompilationSettings& compilationSettings, ShaderCompilationMetaData& outCompilationMetaData) const
{
	SPT_PROFILER_FUNCTION();

	return m_impl->CompileShader(shaderPath, sourceCode, stageCompilationDef, compilationSettings, outCompilationMetaData);
}

} // spt::sc
