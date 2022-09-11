#include "CompilationErrorsLogger.h"
#include "ShaderCompilationEnvironment.h"
#include "ShaderCompilationInput.h"

#include <filesystem>
#include <fstream>
#include "FileSystem/File.h"

namespace spt::sc
{

namespace priv
{

lib::String getShaderStageName(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::None:		return "None";
	case rhi::EShaderStage::Vertex:		return "Vertex";
	case rhi::EShaderStage::Fragment:	return "Fragment";
	case rhi::EShaderStage::Compute:	return "Compute";
	}

	SPT_CHECK_NO_ENTRY(); // Invalid shader type
	return lib::String();
}


class LoggerStreamGuard
{
public:
	
	explicit LoggerStreamGuard(const lib::String& filePath)
	{
		m_stream = lib::File::OpenOutputStream(filePath, lib::Flags(lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::ForceCreate));
	}

	~LoggerStreamGuard()
	{
		m_stream.close();
	}

	std::ofstream& getStream()
	{
		return m_stream;
	}

private:

	std::ofstream		m_stream;
};

}

void CompilationErrorsLogger::OutputShaderPreprocessedCode(const lib::String& shaderPath, const ShaderSourceCode& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	const lib::String logFilePath = GetShaderPreprocessedCodeLogsPath(shaderPath, sourceCode);
	priv::LoggerStreamGuard streamGuard(logFilePath);

	std::ofstream& stream = streamGuard.getStream();

	stream << sourceCode.GetSourceCode();
}

void CompilationErrorsLogger::OutputShaderPreprocessingErrors(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const lib::String& errors)
{
	SPT_PROFILER_FUNCTION();

	const lib::String logFilePath = GetShaderPreprocesingErrorsLogsPath(shaderPath, sourceCode);
	priv::LoggerStreamGuard streamGuard(logFilePath);

	std::ofstream& stream = streamGuard.getStream();

	stream << errors;
}

void CompilationErrorsLogger::OutputShaderCompilationErrors(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const lib::String& errors)
{
	SPT_PROFILER_FUNCTION();

	const lib::String logFilePath = GetShaderCompilationErrorsLogsPath(shaderPath, sourceCode);
	priv::LoggerStreamGuard streamGuard(logFilePath);

	std::ofstream& stream = streamGuard.getStream();

	stream << errors;
}

lib::String CompilationErrorsLogger::GetShaderLogsPath(const lib::String& shaderPath, const ShaderSourceCode& sourceCode)
{
	const lib::String& logsPath = ShaderCompilationEnvironment::GetErrorLogsPath();

	return logsPath + "/" + lib::File::DiscardExtension(shaderPath) + "/" + priv::getShaderStageName(sourceCode.GetShaderStage());
}

lib::String CompilationErrorsLogger::GetShaderPreprocessedCodeLogsPath(const lib::String& shaderPath, const ShaderSourceCode& sourceCode)
{
	return GetShaderLogsPath(shaderPath, sourceCode) + "/Preprocessed." + lib::File::GetExtension(shaderPath);
}

lib::String CompilationErrorsLogger::GetShaderPreprocesingErrorsLogsPath(const lib::String& shaderPath, const ShaderSourceCode& sourceCode)
{
	return GetShaderLogsPath(shaderPath, sourceCode) + "/PreprocessingErrors.log";
}

lib::String CompilationErrorsLogger::GetShaderCompilationErrorsLogsPath(const lib::String& shaderPath, const ShaderSourceCode& sourceCode)
{
	return GetShaderLogsPath(shaderPath, sourceCode) + "/CompilationErrors.log";
}

}
