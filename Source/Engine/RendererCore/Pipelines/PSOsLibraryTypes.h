#pragma once

#include "SculptorCoreTypes.h"
#include "PipelineState.h"
#include "Common/ShaderCompilationInput.h"
#include "Utility/Templates/Callable.h"
#include "ShaderStructsTypes.h"


namespace spt::rdr
{

#define SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION 1

SPT_DEFINE_LOG_CATEGORY(PSOsLibrary, true);


namespace permutations
{

template<typename TShaderStructMemberMetaData>
constexpr SizeType ComputeHashImpl(lib::Span<const Byte> domainData)
{
	if constexpr (rdr::shader_translator::priv::IsHeadMember<TShaderStructMemberMetaData>())
	{
		return ComputeHashImpl<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(domainData);
	}
	else
	{
		using MemberType = typename TShaderStructMemberMetaData::UnderlyingType;

		static_assert(std::is_same_v<MemberType, Int32> || std::is_same_v<MemberType, Bool> || shader_translator::CShaderStruct<MemberType>);

		const MemberType& memberValue = *reinterpret_cast<const MemberType*>(domainData.data() + TShaderStructMemberMetaData::GetCPPOffset());

		if constexpr (rdr::shader_translator::priv::IsTailMember<TShaderStructMemberMetaData>())
		{
			if constexpr (shader_translator::CShaderStruct<MemberType>)
			{
				return ComputeHashImpl<typename MemberType::HeadMemberMetaData>(lib::Span<const Byte>(reinterpret_cast<const Byte*>(&memberValue), sizeof(MemberType)));
			}
			else
			{
				return lib::GetHash(memberValue);
			}
		}
		else
		{
			if constexpr (shader_translator::CShaderStruct<MemberType>)
			{
				return lib::HashCombine(ComputeHashImpl<typename MemberType::HeadMemberMetaData>(lib::Span<const Byte>(reinterpret_cast<const Byte*>(&memberValue), sizeof(MemberType))),
										ComputeHashImpl<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(domainData));
			}
			else
			{
				return lib::HashCombine(memberValue, ComputeHashImpl<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(domainData));
			}
		}
	}
}

template<typename TDomain>
SizeType ComputePermutationHash(const TDomain& permutation)
{
	const lib::Span<const Byte> domainData(reinterpret_cast<const Byte*>(&permutation), sizeof(TDomain));
	return ComputeHashImpl<typename TDomain::HeadMemberMetaData>(domainData);
}


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

		if constexpr (std::is_same_v<MemberType, Bool>)
		{
			if (memberValue)
			{
				outSettings.AddMacroDefinition(sc::MacroDefinition(macroName, memberValue));
			}
		}
		else if constexpr (std::is_same_v<MemberType, Int32>)
		{
			outSettings.AddMacroDefinition(macroName.ToString() + "=" + std::to_string(memberValue));
		}
	}
}

template<typename TDomain>
sc::ShaderCompilationSettings BuildPermutationShaderCompilationSettings(const TDomain& permutation)
{
	sc::ShaderCompilationSettings settings;

	BuildPermutationCompilationSettingsImpl<typename TDomain::HeadMemberMetaData>(lib::Span<const Byte>(reinterpret_cast<const Byte*>(&permutation), sizeof(TDomain)), settings);

	return settings;
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

		const SizeType permutationHash = permutations::ComputePermutationHash<TDomain>(permutation);
		m_permutations.emplace(permutationHash, psoID);
	}

	PipelineStateID GetPermutation(const TDomain& permutation) const
	{
#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
		const lib::LockGuard lock(m_lock); // Lock needed only in runtime. Precaching is thread-safe by design
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION

		const SizeType permutationHash = permutations::ComputePermutationHash<TDomain>(permutation);
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

	virtual rdr::ShaderID CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) = 0;

	virtual rdr::PipelineStateID CreateComputePipeline(const RendererResourceName& name, const rdr::ShaderID& shader) = 0;
	virtual rdr::PipelineStateID CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) = 0;
};


class PSOImmediateCompiler : public PSOCompilerInterface
{
public:

	virtual rdr::ShaderID CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) override;

	virtual rdr::PipelineStateID CreateComputePipeline(const RendererResourceName& name, const rdr::ShaderID& shader) override;
	virtual rdr::PipelineStateID CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) override;
};


using PSOPrecacheFunction = lib::RawCallable<void(PSOCompilerInterface& /*compiler*/, const PSOPrecacheParams& /*params*/)>;
RENDERER_CORE_API void RegisterPSO(PSOPrecacheFunction callable);


struct ShaderEntry
{
	const char* path;
	const char* entryPoint;
};


template<typename TPSO>
concept CPermutationsComputePSO = requires
{
	{ TPSO::s_permutations };
	typename TPSO::PermutationDomainType;
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
	static rdr::PipelineStateID GetPermutation(const TPermutationDomatin& permutation)
	{
		static_assert(CPermutationsComputePSO<TConcrete>, "This PSO does not support permutations!");

		static_assert(std::is_same_v<typename TConcrete::PermutationDomainType, TPermutationDomatin>, "Wrong type of permutation domain!");

#if SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION
		rdr::PipelineStateID pso = TConcrete::s_permutations.GetPermutation(permutation);
		if(!pso.IsValid())
		{
			SPT_LOG_WARN(PSOsLibrary, "PSO '{}' permutation not found! Compiling at runtime...", name.Get());

			PSOImmediateCompiler compiler;
			pso = TConcrete::CompilePSO(compiler, rdr::permutations::BuildPermutationShaderCompilationSettings(permutation));
			TConcrete::AddPermutation(permutation, pso);
		}
#else
		const rdr::PipelineStateID pso = TConcrete::s_permutations.GetPermutation(permutation);
#endif // SPT_ALLOW_RUNTIME_PERMUTATION_COMPILATION

		SPT_CHECK_MSG(pso.IsValid(), "Invalid permutation!");
		return pso;
	}

	template<typename TPermutationDomatin>
	static void AddPermutation(const TPermutationDomatin& permutation, spt::rdr::PipelineStateID psoID)
	{
		static_assert(CPermutationsComputePSO<TConcrete>, "This PSO does not support permutations!");

		static_assert(std::is_same_v<typename TConcrete::PermutationDomainType, TPermutationDomatin>, "Wrong type of permutation domain!");

		TConcrete::s_permutations.AddPermutation(permutation, psoID);
	}

	template<typename TPermutationDomatin>
	static void CompilePermutation(spt::rdr::PSOCompilerInterface& compiler, const TPermutationDomatin& permutation)
	{
		static_assert(CPermutationsComputePSO<TConcrete>, "This PSO does not support permutations!");

		static_assert(std::is_same_v<typename TConcrete::PermutationDomainType, TPermutationDomatin>, "Wrong type of permutation domain!");

		TConcrete::s_permutations.AddPermutation(permutation, TConcrete::CompilePSO(compiler, rdr::permutations::BuildPermutationShaderCompilationSettings(permutation)));
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

#define PERMUTATION_DOMAIN(domain) \
static inline spt::rdr::PSOPermutationsContainer<domain> s_permutations; \
using PermutationDomainType = domain;

} // spt::rdr