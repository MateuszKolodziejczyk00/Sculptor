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
		return lib::HashCombine(vertexShader.GetID(), fragmentShader.GetID());
	}

	ShaderID vertexShader;
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
		: ID(idxNone<SizeType>)
	{ }

	explicit PipelineStateID(SizeType inID)
		: ID(inID)
	{ }

	void Reset()
	{
		ID = idxNone<SizeType>;
	}

	Bool IsValid() const
	{
		return ID != idxNone<SizeType>;
	}

	SizeType GetID() const
	{
		return ID;
	}

	friend auto operator<=>(const PipelineStateID& rhs, const PipelineStateID& lhs) = default;

private:

	SizeType ID;
};

} // spt::rdr


namespace std
{

template<>
struct hash<spt::rdr::PipelineStateID>
{
	constexpr size_t operator()(const spt::rdr::PipelineStateID& state) const
	{
		return spt::lib::GetHash(state.GetID());
	}
};

} // std
