#pragma once

#include "SculptorCoreTypes.h"
#include "Shaders/ShaderTypes.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::rdr
{

struct GraphicsPipelineState
{
	GraphicsPipelineState() = default;

	rhi::EPrimitiveTopology					primitiveTopology;
	rhi::PipelineRasterizationDefinition	rasterizationDefinition;
	ShaderID								shader;
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
