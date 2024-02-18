#pragma once

#include "SculptorCoreTypes.h"
#include "RHI/RHICore/RHIShaderTypes.h"
#include "RHI/RHICore/RHIDescriptorTypes.h"
#include "RHI/RHICore/RHISamplerTypes.h"

#include <variant>


namespace spt::smd
{

// should be in the same order as BindingDataVariant
enum class EBindingType : Uint32
{
	Texture,
	CombinedTextureSampler,
	Buffer,
	Sampler,
	AccelerationStructure,

	NUM
};


enum class EBindingFlags : Flags32
{
	None				= 0,
	Invalid				= BIT(0),

	Unbound				= BIT(1),
	DynamicOffset		= BIT(2),
	TexelBuffer			= BIT(3),
	Storage				= BIT(4),
	
	PartiallyBound		= BIT(5),

	ImmutableSampler	= BIT(6),

	// Shader stages
	VertexShader		= BIT(21),
	FragmentShader		= BIT(22),
	ComputeShader		= BIT(23),
	RTGeneration		= BIT(24),
	RTAnyHit			= BIT(25),
	RTClosestHit		= BIT(26),
	RTMiss				= BIT(27),
	RTIntersection		= BIT(28),

	AllShaders			= VertexShader | FragmentShader | ComputeShader | RTGeneration | RTAnyHit | RTClosestHit | RTMiss | RTIntersection
};

} // spt::smd
