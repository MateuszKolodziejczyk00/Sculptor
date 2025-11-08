#pragma once

#include "SculptorCoreTypes.h"
#include "PipelineState.h"
#include "Common/ShaderCompilationInput.h"
#include "Utility/Templates/Callable.h"


namespace spt::rdr
{


struct PSOPrecacheParams
{

};


class PSOCompilerInterface
{
public:

	virtual rdr::ShaderID CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) = 0;

	virtual rdr::PipelineStateID CreateComputePipeline(const RendererResourceName& name, const rdr::ShaderID& shader) = 0;
	virtual rdr::PipelineStateID CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) = 0;
};

using PSOPrecacheFunction = lib::RawCallable<void(PSOCompilerInterface& /*compiler*/, const PSOPrecacheParams& /*params*/)>;
RENDERER_CORE_API void RegisterPSO(PSOPrecacheFunction callable);


struct ShaderEntry
{
	const char* path;
	const char* entryPoint;
};


template<typename TConcrete, lib::Literal name>
class ComputePSO
{
public:

	static PipelineStateID CompilePSO(PSOCompilerInterface& compiler, const sc::ShaderCompilationSettings& settings)
	{
		const ShaderEntry computeShader = TConcrete::computeShader;
		const rdr::ShaderID shaderID = compiler.CompileShader( computeShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, computeShader.entryPoint), settings);
		return compiler.CreateComputePipeline(RENDERER_RESOURCE_NAME(name.Get()), shaderID);
	}

private:

	struct Registrar
	{
		Registrar()
		{
			PSOPrecacheFunction func = TConcrete::PrecachePSOs;
			RegisterPSO(func);
		}
	};

	static inline Registrar registrar{};
};


template<typename TConcrete, lib::Literal name>
class GraphicsPSO
{
public:

	static PipelineStateID CompilePSO(PSOCompilerInterface& compiler, const rhi::GraphicsPipelineDefinition& pipelineDef, const sc::ShaderCompilationSettings& settings)
	{
		rdr::GraphicsPipelineShaders shaders;

		if constexpr (requires { TConcrete::taskShader; })
		{
			const ShaderEntry taskShader = TConcrete::taskShader;
			shaders.taskShader = compiler.CompileShader( taskShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Task, taskShader.entryPoint), settings);
		}

		if constexpr (requires { TConcrete::meshShader; })
		{
			const ShaderEntry meshShader = TConcrete::meshShader;
			shaders.meshShader = compiler.CompileShader( meshShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Mesh, meshShader.entryPoint), settings);
		}

		if constexpr (requires { TConcrete::vertexShader; })
		{
			const ShaderEntry vertexShader = TConcrete::vertexShader;
			shaders.vertexShader = compiler.CompileShader( vertexShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, vertexShader.entryPoint), settings);
		}

		const ShaderEntry fragmentShader = TConcrete::fragmentShader;
		shaders.fragmentShader = compiler.CompileShader(fragmentShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, fragmentShader.entryPoint), settings);

		return compiler.CreateGraphicsPipeline(RENDERER_RESOURCE_NAME(name.Get()), shaders, pipelineDef);
	}

private:

	struct Registrar
	{
		Registrar()
		{
			PSOPrecacheFunction func = TConcrete::PrecachePSOs;
			RegisterPSO(func);
		}
	};

	static inline Registrar registrar{};
};


class PSOPreset
{
public:

	PSOPreset() = default;

	explicit PSOPreset(PipelineStateID psoID)
		: m_psoID(psoID)
	{
	}

	PSOPreset& operator=(PipelineStateID psoID)
	{
		m_psoID = psoID;
		return *this;
	}

	operator PipelineStateID() const
	{
		return m_psoID;
	}

private:

	PipelineStateID m_psoID;
};


#define COMPUTE_PSO(PSOName) struct PSOName : public spt::rdr::ComputePSO<PSOName, #PSOName>

#define GRAPHICS_PSO(PSOName) struct PSOName : public spt::rdr::GraphicsPSO<PSOName, #PSOName>

#define COMPUTE_SHADER(path, entryPoint)  static constexpr spt::rdr::ShaderEntry computeShader{ path, entryPoint };
#define VERTEX_SHADER(path, entryPoint)   static constexpr spt::rdr::ShaderEntry vertexShader{ path, entryPoint };
#define FRAGMENT_SHADER(path, entryPoint) static constexpr spt::rdr::ShaderEntry fragmentShader{ path, entryPoint };
#define TASK_SHADER(path, entryPoint)     static constexpr spt::rdr::ShaderEntry taskShader{ path, entryPoint };
#define MESH_SHADER(path, entryPoint)     static constexpr spt::rdr::ShaderEntry meshShader{ path, entryPoint };


#define PRESET(name) static inline spt::rdr::PSOPreset name;

} // spt::rdr