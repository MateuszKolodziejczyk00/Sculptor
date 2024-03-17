#include "CompilationErrorsLogger.h"
#include "ShaderCompilationEnvironment.h"
#include "ShaderCompilationInput.h"

#include <filesystem>
#include <fstream>
#include "FileSystem/File.h"

namespace spt::sc
{

SPT_DEFINE_LOG_CATEGORY(CompilationErrorsLogger, true)

namespace priv
{

lib::String GetShaderStageName(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::None:			return "None";
	case rhi::EShaderStage::Vertex:			return "Vertex";
	case rhi::EShaderStage::Task:			return "Task";
	case rhi::EShaderStage::Mesh:			return "Mesh";
	case rhi::EShaderStage::Fragment:		return "Fragment";
	case rhi::EShaderStage::Compute:		return "Compute";
	case rhi::EShaderStage::RTGeneration:	return "RTGeneration";
	case rhi::EShaderStage::RTAnyHit:		return "RTAnyHit";
	case rhi::EShaderStage::RTClosestHit:	return "RTClosestHit";
	case rhi::EShaderStage::RTMiss:			return "RTMiss";
	case rhi::EShaderStage::RTIntersection:	return "RTIntersection";
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

void CompilationErrorsLogger::OutputShaderPreprocessedCode(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef)
{
	SPT_PROFILER_FUNCTION();

	const lib::String logFilePath = GetShaderPreprocessedCodeLogsPath(shaderPath, sourceCode, stageCompilationDef);
	priv::LoggerStreamGuard streamGuard(logFilePath);

	std::ofstream& stream = streamGuard.getStream();

	stream << sourceCode;
}

void CompilationErrorsLogger::OutputShaderPreprocessingErrors(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const lib::String& errors)
{
	SPT_PROFILER_FUNCTION();

	SPT_LOG_ERROR(CompilationErrorsLogger, "Compiling {} failed:\n{}", shaderPath, errors);

	const lib::String logFilePath = GetShaderPreprocesingErrorsLogsPath(shaderPath, sourceCode, stageCompilationDef);
	priv::LoggerStreamGuard streamGuard(logFilePath);

	std::ofstream& stream = streamGuard.getStream();

	stream << errors;
}

void CompilationErrorsLogger::OutputShaderCompilationErrors(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const lib::String& errors)
{
	SPT_PROFILER_FUNCTION();

	const lib::String logFilePath = GetShaderCompilationErrorsLogsPath(shaderPath, sourceCode, stageCompilationDef);
	priv::LoggerStreamGuard streamGuard(logFilePath);

	std::ofstream& stream = streamGuard.getStream();

	stream << errors;
}

lib::String CompilationErrorsLogger::GetShaderLogsPath(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef)
{
	const lib::String& logsPath = ShaderCompilationEnvironment::GetErrorLogsPath();

	return logsPath + '/' + lib::File::DiscardExtension(shaderPath) + '/' + priv::GetShaderStageName(stageCompilationDef.stage) + '/' + stageCompilationDef.entryPoint.GetData();
}

lib::String CompilationErrorsLogger::GetShaderPreprocessedCodeLogsPath(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef)
{
	return GetShaderLogsPath(shaderPath, sourceCode, stageCompilationDef) + "/Preprocessed." + lib::File::GetExtension(shaderPath);
}

lib::String CompilationErrorsLogger::GetShaderPreprocesingErrorsLogsPath(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef)
{
	return GetShaderLogsPath(shaderPath, sourceCode, stageCompilationDef) + "/PreprocessingErrors.log";
}

lib::String CompilationErrorsLogger::GetShaderCompilationErrorsLogsPath(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef)
{
	return GetShaderLogsPath(shaderPath, sourceCode, stageCompilationDef) + "/CompilationErrors.log";
}

} // spt::sc
