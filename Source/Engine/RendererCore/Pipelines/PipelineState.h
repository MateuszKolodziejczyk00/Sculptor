#pragma once

#include "SculptorCoreTypes.h"
#include "Shaders/ShaderTypes.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::rdr
{

struct GraphicsPipelineShaders
{
	GraphicsPipelineShaders() = default;

	SizeType Hash() const
	{
		return lib::HashCombine(vertexShader.GetID(), taskShader.GetID(), meshShader.GetID(), fragmentShader.GetID());
	}

	ShaderID vertexShader;
	ShaderID taskShader;
	ShaderID meshShader;
	ShaderID fragmentShader;
};


struct RayTracingHitGroup
{
	RayTracingHitGroup() = default;

	RayTracingHitGroup(ShaderID inClosestHitShader)
		: closestHitShader(inClosestHitShader)
	{ }

	SizeType Hash() const
	{
		return lib::HashCombine(closestHitShader, anyHitShader, intersectionShader);
	}

	ShaderID closestHitShader;
	ShaderID anyHitShader;
	ShaderID intersectionShader;
};


struct RayTracingPipelineShaders
{
	RayTracingPipelineShaders() = default;

	SizeType Hash() const
	{
		return lib::HashCombine(rayGenShader.GetID(),
								lib::HashRange(std::cbegin(hitGroups), std::cend(hitGroups),
											   [](const RayTracingHitGroup& hitGroup)
											   {
												   return hitGroup.Hash();
											   }),
								lib::HashRange(std::cbegin(missShaders), std::cend(missShaders)));
	}

	ShaderID rayGenShader;
	lib::DynamicArray<RayTracingHitGroup> hitGroups;
	lib::DynamicArray<ShaderID> missShaders;
};


struct PipelineStateID
{
public:

	PipelineStateID()
		: m_ID(idxNone<SizeType>)
		, m_type(rhi::EPipelineType::None)
	{ }

	explicit PipelineStateID(SizeType inID, rhi::EPipelineType inType)
		: m_ID(inID)
		, m_type(inType)
	{ }

	void Reset()
	{
		m_ID   = idxNone<SizeType>;
		m_type = rhi::EPipelineType::None;
	}

	Bool IsValid() const
	{
		return m_ID != idxNone<SizeType>;
	}

	SizeType GetID() const
	{
		return m_ID;
	}

	rhi::EPipelineType GetPipelineType() const
	{
		return m_type;
	}

	friend auto operator<=>(const PipelineStateID& rhs, const PipelineStateID& lhs) = default;

private:

	SizeType           m_ID;
	rhi::EPipelineType m_type;
};

} // spt::rdr


namespace spt::lib
{

template<>
struct Hasher<rdr::PipelineStateID>
{
	constexpr size_t operator()(const rdr::PipelineStateID& state) const
	{
		return GetHash(state.GetID());
	}
};

} // spt::lib
