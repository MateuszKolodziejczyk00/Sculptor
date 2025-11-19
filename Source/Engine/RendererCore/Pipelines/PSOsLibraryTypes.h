#pragma once

#include "SculptorCoreTypes.h"
#include "PipelineState.h"
#include "Common/ShaderCompilationInput.h"
#include "Utility/Templates/Callable.h"
#include "ShaderStructs.h"


namespace spt::rdr
{

#define SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION 1

SPT_DEFINE_LOG_CATEGORY(PSOsLibrary, true);


namespace permutations
{

template<typename TType>
struct ShaderCompilationSettingsBuilder
{
	static void Build(lib::String variableName, const TType& value, sc::ShaderCompilationSettings& outSettings)
	{
		SPT_CHECK_NO_ENTRY(); // type not supported
	}
};

template<>
struct ShaderCompilationSettingsBuilder<Int32>
{
	static void Build(const lib::String& variableName, const Int32& value, sc::ShaderCompilationSettings& outSettings)
	{
		outSettings.AddMacroDefinition(sc::MacroDefinition(variableName + "=" + std::to_string(value)));
	}
};

template<>
struct ShaderCompilationSettingsBuilder<Bool>
{
	static void Build(const lib::String& variableName, const Bool& value, sc::ShaderCompilationSettings& outSettings)
	{
		outSettings.AddMacroDefinition(sc::MacroDefinition(std::move(variableName), value));
	}
};

template<>
struct ShaderCompilationSettingsBuilder<lib::HashedString>
{
	static void Build(const lib::String& variableName, const lib::HashedString& value, sc::ShaderCompilationSettings& outSettings)
	{
		outSettings.AddMacroDefinition(sc::MacroDefinition(variableName + "=" + value.ToString()));
	}
};


template<typename TShaderStructMemberMetaData>
constexpr void BuildPermutationCompilationSettingsImpl(lib::Span<const Byte> domainData, sc::ShaderCompilationSettings& outSettings)
{
	if constexpr (!rdr::shader_translator::priv::IsTailMember<TShaderStructMemberMetaData>())
	{
		BuildPermutationCompilationSettingsImpl<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(domainData, outSettings);
	}

	if constexpr (!rdr::shader_translator::priv::IsHeadMember<TShaderStructMemberMetaData>())
	{
		using MemberType = typename TShaderStructMemberMetaData::UnderlyingType;

		const MemberType& memberValue = *reinterpret_cast<const MemberType*>(domainData.data() + TShaderStructMemberMetaData::GetCPPOffset());

		const lib::HashedString macroName = lib::HashedString(TShaderStructMemberMetaData::GetVariableName());

		if constexpr (shader_translator::CShaderStruct<MemberType>)
		{
			BuildPermutationCompilationSettingsImpl<typename MemberType::HeadMemberMetaData>(lib::Span<const Byte>(reinterpret_cast<const Byte*>(&memberValue), sizeof(MemberType)), outSettings);
		}
		else
		{
			ShaderCompilationSettingsBuilder<MemberType>::Build(macroName.ToString(), memberValue, INOUT outSettings);
		}
	}
}


template<typename TDomain>
void BuildPermutationShaderCompilationSettings(const TDomain& permutation, sc::ShaderCompilationSettings& inOutSettings)
{
	BuildPermutationCompilationSettingsImpl<typename TDomain::HeadMemberMetaData>(lib::Span<const Byte>(reinterpret_cast<const Byte*>(&permutation), sizeof(TDomain)), inOutSettings);
}

} // permutations


template<typename TDomain>
class PSOPermutationsContainer
{
public:

	PSOPermutationsContainer() = default;

	void AddPermutation(const TDomain& permutation, PipelineStateID psoID)
	{
#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
		const lib::LockGuard lock(m_lock); // Lock needed only in runtime. Precaching is thread-safe by design
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION

		const SizeType permutationHash = ComputeStructHash<TDomain>(permutation);
		m_permutations.emplace(permutationHash, psoID);
	}

	void AddPermutation(const rhi::GraphicsPipelineDefinition& pipelineDef, const TDomain& permutation, PipelineStateID psoID)
	{
#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
		const lib::LockGuard lock(m_lock); // Lock needed only in runtime. Precaching is thread-safe by design
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION

		const SizeType permutationHash = lib::HashCombine(pipelineDef, ComputeStructHash<TDomain>(permutation));
		m_permutations.emplace(permutationHash, psoID);
	}

	PipelineStateID GetPermutation(const TDomain& permutation) const
	{
#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
		const lib::LockGuard lock(m_lock); // Lock needed only in runtime. Precaching is thread-safe by design
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION

		const SizeType permutationHash = ComputeStructHash<TDomain>(permutation);
		const auto foundIt = m_permutations.find(permutationHash);
		if (foundIt != std::cend(m_permutations))
		{
			return foundIt->second;
		}
		return PipelineStateID{};
	}


	PipelineStateID GetPermutation(const rhi::GraphicsPipelineDefinition& pipelineDef, const TDomain& permutation) const
	{
#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
		const lib::LockGuard lock(m_lock); // Lock needed only in runtime. Precaching is thread-safe by design
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION

		const SizeType permutationHash = lib::HashCombine(pipelineDef, ComputeStructHash<TDomain>(permutation));
		const auto foundIt = m_permutations.find(permutationHash);
		if (foundIt != std::cend(m_permutations))
		{
			return foundIt->second;
		}
		return PipelineStateID{};
	}

private:

	lib::HashMap<SizeType, PipelineStateID> m_permutations;

#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
	mutable lib::Lock m_lock;
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
};

struct PSOPrecacheParams
{

};


class RENDERER_CORE_API PSOCompilerInterface
{
public:

	virtual ShaderID CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) = 0;

	virtual PipelineStateID CreateComputePipeline(const RendererResourceName& name, const rdr::ShaderID& shader) = 0;
	virtual PipelineStateID CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) = 0;
	virtual PipelineStateID CreateRayTracingPipeline(const RendererResourceName& name, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef) = 0;
};


class PSOImmediateCompiler : public PSOCompilerInterface
{
public:

	virtual ShaderID CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) override;

	virtual PipelineStateID CreateComputePipeline(const RendererResourceName& name, const rdr::ShaderID& shader) override;
	virtual PipelineStateID CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) override;
	virtual PipelineStateID CreateRayTracingPipeline(const RendererResourceName& name, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef) override;
};


using PSOPrecacheFunction = lib::RawCallable<void(PSOCompilerInterface& /*compiler*/, const PSOPrecacheParams& /*params*/)>;
RENDERER_CORE_API void RegisterPSO(PSOPrecacheFunction callable);


struct ShaderEntry
{
	const char* path;
	const char* entryPoint;
};


template<typename TPSO>
concept CPermutationsPSO = requires
{
	{ TPSO::s_permutations };
	typename TPSO::PermutationDomainType;
};


template<typename TPSO>
concept CPrecachingPSO = requires
{
	{ TPSO::PrecachePSOs };
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

	template<typename TPermutationDomatin>
	static PipelineStateID GetPermutation(const TPermutationDomatin& permutation)
	{
		static_assert(CPermutationsPSO<TConcrete>, "This PSO does not support permutations!");

		static_assert(std::is_same_v<typename TConcrete::PermutationDomainType, TPermutationDomatin>, "Wrong type of permutation domain!");

#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
		PipelineStateID pso = TConcrete::s_permutations.GetPermutation(permutation);
		if(!pso.IsValid())
		{
			SPT_LOG_WARN(PSOsLibrary, "PSO '{}' permutation not found! Compiling at runtime...", name.Get());

			sc::ShaderCompilationSettings compilationSettings;
			rdr::permutations::BuildPermutationShaderCompilationSettings(permutation, INOUT compilationSettings);

			PSOImmediateCompiler compiler;
			pso = TConcrete::CompilePSO(compiler, compilationSettings);
			TConcrete::s_permutations.AddPermutation(permutation, pso);
		}
#else
		const PipelineStateID pso = TConcrete::s_permutations.GetPermutation(permutation);
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION

		SPT_CHECK_MSG(pso.IsValid(), "Invalid permutation!");
		return pso;
	}

	template<typename TPermutationDomatin>
	static PipelineStateID CompilePermutation(spt::rdr::PSOCompilerInterface& compiler, const TPermutationDomatin& permutation)
	{
		sc::ShaderCompilationSettings compilationSettings;
		rdr::permutations::BuildPermutationShaderCompilationSettings(permutation, INOUT compilationSettings);

		const PipelineStateID pso = TConcrete::CompilePSO(compiler, compilationSettings);

		if constexpr (CPermutationsPSO<TConcrete>)
		{
			static_assert(std::is_same_v<typename TConcrete::PermutationDomainType, TPermutationDomatin>, "Wrong type of permutation domain!");

			TConcrete::s_permutations.AddPermutation(permutation, pso);
		}

		return pso;
	}

private:

	struct Registrar
	{
		Registrar()
		{
			if constexpr (CPrecachingPSO<TConcrete>)
			{
				PSOPrecacheFunction func = TConcrete::PrecachePSOs;
				RegisterPSO(func);
			}
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
			shaders.taskShader = compiler.CompileShader(taskShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Task, taskShader.entryPoint), settings);
		}

		if constexpr (requires { TConcrete::meshShader; })
		{
			const ShaderEntry meshShader = TConcrete::meshShader;
			shaders.meshShader = compiler.CompileShader(meshShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Mesh, meshShader.entryPoint), settings);
		}

		if constexpr (requires { TConcrete::vertexShader; })
		{
			const ShaderEntry vertexShader = TConcrete::vertexShader;
			shaders.vertexShader = compiler.CompileShader(vertexShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, vertexShader.entryPoint), settings);
		}

		const ShaderEntry fragmentShader = TConcrete::fragmentShader;
		shaders.fragmentShader = compiler.CompileShader(fragmentShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, fragmentShader.entryPoint), settings);

		return compiler.CreateGraphicsPipeline(RENDERER_RESOURCE_NAME(name.Get()), shaders, pipelineDef);
	}

	template<typename TPermutationDomatin>
	static rdr::PipelineStateID GetPermutation(const rhi::GraphicsPipelineDefinition& pipelineDef, const TPermutationDomatin& permutation)
	{
		static_assert(CPermutationsPSO<TConcrete>, "This PSO does not support permutations!");

		static_assert(std::is_same_v<typename TConcrete::PermutationDomainType, TPermutationDomatin>, "Wrong type of permutation domain!");

#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
		rdr::PipelineStateID pso = TConcrete::s_permutations.GetPermutation(pipelineDef, permutation);
		if (!pso.IsValid())
		{
			SPT_LOG_WARN(PSOsLibrary, "PSO '{}' permutation not found! Compiling at runtime...", name.Get());

			sc::ShaderCompilationSettings compilationSettings;
			rdr::permutations::BuildPermutationShaderCompilationSettings(permutation, INOUT compilationSettings);

			PSOImmediateCompiler compiler;
			pso = TConcrete::CompilePSO(compiler, pipelineDef, compilationSettings);
			TConcrete::s_permutations.AddPermutation(pipelineDef, permutation, pso);
		}
#else
		const rdr::PipelineStateID pso = TConcrete::s_permutations.GetPermutation(pipelineDef, permutation);
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION

		SPT_CHECK_MSG(pso.IsValid(), "Invalid permutation!");
		return pso;
	}

	template<typename TPermutationDomatin>
	static PipelineStateID CompilePermutation(spt::rdr::PSOCompilerInterface& compiler, const rhi::GraphicsPipelineDefinition& pipelineDef, const TPermutationDomatin& permutation)
	{
		sc::ShaderCompilationSettings compilationSettings;
		rdr::permutations::BuildPermutationShaderCompilationSettings(permutation, INOUT compilationSettings);

		const PipelineStateID pso = TConcrete::CompilePSO(compiler, pipelineDef, compilationSettings);

		if constexpr (CPermutationsPSO<TConcrete>)
		{
			static_assert(std::is_same_v<typename TConcrete::PermutationDomainType, TPermutationDomatin>, "Wrong type of permutation domain!");
			TConcrete::s_permutations.AddPermutation(pipelineDef, permutation, pso);
		}

		return pso;
	}

private:

	struct Registrar
	{
		Registrar()
		{
			if constexpr (CPrecachingPSO<TConcrete>)
			{
				PSOPrecacheFunction func = TConcrete::PrecachePSOs;
				RegisterPSO(func);
			}
		}
	};

	static inline Registrar registrar{};
};


template<typename TConcrete, lib::Literal name>
class RayTracingPSO
{
public:

	// Must be a template to defer compilation until concrete pso type is parsed. in parctice, THitGroup must be a TConcrete::HitGroup
	template<typename THitGroup>
	static PipelineStateID CompilePSO(PSOCompilerInterface& compiler, const rhi::RayTracingPipelineDefinition& pipelineDefinition, const lib::DynamicArray<THitGroup>& hitGroups, const sc::ShaderCompilationSettings& settings = {})
	{
		static_assert(std::is_same_v<typename TConcrete::HitGroup, THitGroup>, "Wrong type of hit group!");

		rdr::RayTracingPipelineShaders shaders;

		if constexpr (requires { TConcrete::rayGenShader; })
		{
			shaders.rayGenShader = compiler.CompileShader(TConcrete::rayGenShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, TConcrete::rayGenShader.entryPoint), settings);
		}

		if constexpr (requires { TConcrete::missShaders; })
		{
			shaders.missShaders.reserve(TConcrete::missShaders.size());

			for (const ShaderEntry& missShaderEntry : TConcrete::missShaders)
			{
				shaders.missShaders.push_back(compiler.CompileShader(missShaderEntry.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, missShaderEntry.entryPoint), settings));
			}
		}

		for (const typename TConcrete::HitGroup& hitGroupDef : hitGroups)
		{
			sc::ShaderCompilationSettings hitGroupSettings = settings;

			if constexpr (requires { hitGroupDef.permutation; })
			{
				permutations::BuildPermutationShaderCompilationSettings(hitGroupDef.permutation, INOUT hitGroupSettings);
			}

			rdr::RayTracingHitGroup hitGroup;
			if constexpr (requires { hitGroupDef.closestShader; })
			{
				hitGroup.closestHitShader = compiler.CompileShader(hitGroupDef.closestShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::RTClosestHit, hitGroupDef.closestShader.entryPoint), hitGroupSettings);
			}
			if constexpr (requires { hitGroupDef.anyHitShader; })
			{
				hitGroup.anyHitShader = compiler.CompileShader(hitGroupDef.anyHitShader.path, sc::ShaderStageCompilationDef(rhi::EShaderStage::RTAnyHit, hitGroupDef.anyHitShader.entryPoint), hitGroupSettings);
			}
			shaders.hitGroups.push_back(std::move(hitGroup));
		}

		return compiler.CreateRayTracingPipeline(RENDERER_RESOURCE_NAME(name.Get()), shaders, pipelineDefinition);
	}

private:

	struct Registrar
	{
		Registrar()
		{
			if constexpr (CPrecachingPSO<TConcrete>)
			{
				PSOPrecacheFunction func = TConcrete::PrecachePSOs;
				RegisterPSO(func);
			}
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


template<typename... TShaderEntries>
constexpr auto CreateShaderEntriesArray(TShaderEntries&&... entries)
{
	constexpr SizeType size = lib::ParameterPackSize<TShaderEntries...>::Count;
	return lib::StaticArray<ShaderEntry, size>{ std::forward<TShaderEntries>(entries)... };
}


#define COMPUTE_PSO(PSOName) struct PSOName : public spt::rdr::ComputePSO<PSOName, #PSOName>

#define GRAPHICS_PSO(PSOName) struct PSOName : public spt::rdr::GraphicsPSO<PSOName, #PSOName>

#define RT_PSO(PSOName) struct PSOName : public spt::rdr::RayTracingPSO<PSOName, #PSOName>

#define SHADER_ENTRY(path, entryPoint) spt::rdr::ShaderEntry{path, #entryPoint}

#define COMPUTE_SHADER(path, entryPoint)  static constexpr spt::rdr::ShaderEntry   computeShader{ path, #entryPoint };

#define VERTEX_SHADER(path, entryPoint)   static constexpr spt::rdr::ShaderEntry   vertexShader{ path, #entryPoint };
#define FRAGMENT_SHADER(path, entryPoint) static constexpr spt::rdr::ShaderEntry   fragmentShader{ path, #entryPoint };
#define TASK_SHADER(path, entryPoint)     static constexpr spt::rdr::ShaderEntry   taskShader{ path, #entryPoint };
#define MESH_SHADER(path, entryPoint)     static constexpr spt::rdr::ShaderEntry   meshShader{ path, #entryPoint };

#define RAY_GEN_SHADER(path, entryPoint)  static constexpr spt::rdr::ShaderEntry   rayGenShader{ path, #entryPoint };
#define MISS_SHADERS(...)                 static constexpr auto                    missShaders = spt::rdr::CreateShaderEntriesArray(__VA_ARGS__);
#define HIT_GROUP                         struct HitGroup

#define CLOSEST_HIT_SHADER(path, entryPoint)  static constexpr spt::rdr::ShaderEntry   closestShader{ path, #entryPoint };
#define ANY_HIT_SHADER(path, entryPoint) \
static constexpr spt::rdr::ShaderEntry   anyHitShader{ path, #entryPoint }; \
Bool hasAnyHitShader = true;


#define HIT_PERMUTATION_DOMAIN(domain) \
domain permutation; \
using PermutationDomainType = domain;


#define PRESET(name) static inline spt::rdr::PSOPreset name


#define PERMUTATION_DOMAIN(domain) \
static inline spt::rdr::PSOPermutationsContainer<domain> s_permutations; \
using PermutationDomainType = domain;


class DebugFeature
{
public:

	DebugFeature& operator=(Bool enabled)
	{
		m_enabled = enabled;
		return *this;
	}

	operator Bool() const
	{
		return m_enabled;
	}

private:

	Bool m_enabled = false;
};


namespace shader_translator
{

template<>
struct StructTranslator<DebugFeature>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return StructTranslator<Bool>::GetHLSLStructName();
	}
};

template<>
struct StructCPPToHLSLTranslator<DebugFeature>
{
	static void Copy(const DebugFeature& cppData, lib::Span<Byte> hlslData)
	{
		// Doesn't matter, Materials Hash is used only for permutations identification, not as shaders input
		StructCPPToHLSLTranslator<Bool>::Copy(Bool(cppData), hlslData);
	}
};

template<>
struct StructHLSLSizeEvaluator<DebugFeature>
{
	static constexpr Uint32 Size()
	{
		return StructHLSLSizeEvaluator<Bool>::Size();
	}
};

template<>
struct StructHLSLAlignmentEvaluator<DebugFeature>
{
	static constexpr Uint32 Alignment()
	{
		return StructHLSLAlignmentEvaluator<Bool>::Alignment();
	}
};


} // shader_translator

namespace permutations
{

template<>
struct ShaderCompilationSettingsBuilder<DebugFeature>
{
	static void Build(const lib::String& variableName, const DebugFeature& value, sc::ShaderCompilationSettings& outSettings)
	{
		ShaderCompilationSettingsBuilder<Bool>::Build(variableName, Bool(value), outSettings);

		if (value)
		{
			outSettings.AddMacroDefinition(sc::MacroDefinition("SPT_META_PARAM_DEBUG_FEATURES", true));
		}
	}
};

} // permutations

} // spt::rdr


namespace std
{

template<>
struct hash<spt::rdr::DebugFeature>
{
	size_t operator()(const spt::rdr::DebugFeature& feature) const
	{
		return spt::lib::GetHash(spt::Bool(feature));
	}
};

} // std
