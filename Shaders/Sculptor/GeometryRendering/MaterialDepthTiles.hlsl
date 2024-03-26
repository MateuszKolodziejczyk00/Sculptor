#include "SculptorShader.hlsli"

[[descriptor_set(CreateMaterialDepthTilesDS, 0)]]

#include "GeometryRendering/GeometryCommon.hlsli"


groupshared uint tileValues[32][32];


uint MinMaterialBatchIdx(uint lhs, uint rhs)
{
	return min(lhs, rhs);
}


uint MaxMaterialBatchIdx(uint lhs, uint rhs)
{
	const uint value = max(lhs, rhs);
	return value == 0xFFFF ? MinMaterialBatchIdx(lhs, rhs) : value;
}


uint MinMaterialBatchIdx(uint4 materialBatchIndices)
{
	const uint a = MinMaterialBatchIdx(materialBatchIndices.x, materialBatchIndices.y);
	const uint b = MinMaterialBatchIdx(materialBatchIndices.z, materialBatchIndices.w);
	return MinMaterialBatchIdx(a, b);
}


uint MaxMaterialBatchIdx(uint4 materialBatchIndices)
{
	const uint a = MaxMaterialBatchIdx(materialBatchIndices.x, materialBatchIndices.y);
	const uint b = MaxMaterialBatchIdx(materialBatchIndices.z, materialBatchIndices.w);
	return MaxMaterialBatchIdx(a, b);
}


uint PackBatchIndices(uint minMaterialBatchIdx, uint maxMaterialBatchIdx)
{
	return (minMaterialBatchIdx & 0xFFFF) | (maxMaterialBatchIdx << 16);
}


void UnpackBatchIndices(uint packedBatchIndices, out uint minMaterialBatchIdx, out uint maxMaterialBatchIdx)
{
	minMaterialBatchIdx = packedBatchIndices & 0xFFFF;
	maxMaterialBatchIdx = packedBatchIndices >> 16;
}


struct ReductionState
{
	static ReductionState Create(in uint2 inLocalPixel)
	{
		ReductionState state;
		state.localPixel             = inLocalPixel;
		state.reducedPixel           = inLocalPixel;
		state.isRelevantInvocation = true;
		return state;
	}

	uint2 localPixel;
	uint2 reducedPixel;
	bool isRelevantInvocation;
};


void Reduce(inout ReductionState reductionState, uint iterationIdx)
{
	uint reduced = 0xFFFFFFFF;

	const uint relevanceMask = (2u << iterationIdx) - 1u;

	reductionState.isRelevantInvocation = ((reductionState.localPixel.x | reductionState.localPixel.y) & relevanceMask) == 0;
	if(reductionState.isRelevantInvocation)
	{
		const uint4 ranges = uint4(
			tileValues[reductionState.reducedPixel.x][reductionState.reducedPixel.y],
			tileValues[reductionState.reducedPixel.x][reductionState.reducedPixel.y + 1],
			tileValues[reductionState.reducedPixel.x + 1][reductionState.reducedPixel.y],
			tileValues[reductionState.reducedPixel.x + 1][reductionState.reducedPixel.y + 1]);

		uint4 minMaterialBatchIndices;
		uint4 maxMaterialBatchIndices;

		UnpackBatchIndices(ranges.x, minMaterialBatchIndices.x, maxMaterialBatchIndices.x);
		UnpackBatchIndices(ranges.y, minMaterialBatchIndices.y, maxMaterialBatchIndices.y);
		UnpackBatchIndices(ranges.z, minMaterialBatchIndices.z, maxMaterialBatchIndices.z);
		UnpackBatchIndices(ranges.w, minMaterialBatchIndices.w, maxMaterialBatchIndices.w);

		const uint minMaterialBatchIdx = MinMaterialBatchIdx(minMaterialBatchIndices);
		const uint maxMaterialBatchIdx = MaxMaterialBatchIdx(maxMaterialBatchIndices);

		reduced = PackBatchIndices(minMaterialBatchIdx, maxMaterialBatchIdx);
	}

	reductionState.reducedPixel >>= 1;
	
	GroupMemoryBarrierWithGroupSync();
	if(reductionState.isRelevantInvocation)
	{
		tileValues[reductionState.reducedPixel.x][reductionState.reducedPixel.y] = reduced;
	}
	GroupMemoryBarrierWithGroupSync();
}


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 localID  : SV_GroupThreadID;
	uint3 groupID  : SV_GroupID;
};


[numthreads(32, 32, 1)]
void MaterialDepthTilesCS(CS_INPUT input)
{
	const uint2 groupOffset = input.groupID.xy * 64u;
	const uint2 pixel = min(u_params.materialDepthResolution - 2, groupOffset + 2 * input.localID.xy);

	float4 materialDepth = 1.f;
	materialDepth.x = u_materialDepthTexture.Load(uint3(pixel + uint2(0, 0), 0));
	materialDepth.y = u_materialDepthTexture.Load(uint3(pixel + uint2(0, 1), 0));
	materialDepth.z = u_materialDepthTexture.Load(uint3(pixel + uint2(1, 0), 0));
	materialDepth.w = u_materialDepthTexture.Load(uint3(pixel + uint2(1, 1), 0));

	const uint4 materialBatchIdx = uint4(
		MaterialDepthToMaterialBatchIdx(materialDepth.x),
		MaterialDepthToMaterialBatchIdx(materialDepth.y),
		MaterialDepthToMaterialBatchIdx(materialDepth.z),
		MaterialDepthToMaterialBatchIdx(materialDepth.w));

	const uint minMaterialBatchIdx = MinMaterialBatchIdx(materialBatchIdx);
	const uint maxMaterialBatchIdx = MaxMaterialBatchIdx(materialBatchIdx);

	tileValues[input.localID.x][input.localID.y] = PackBatchIndices(minMaterialBatchIdx, maxMaterialBatchIdx);

	ReductionState reductionState = ReductionState::Create(input.localID.xy);

	GroupMemoryBarrierWithGroupSync();

	for(uint iterationIdx = 0; iterationIdx < 5; ++iterationIdx)
	{
		Reduce(INOUT reductionState, iterationIdx);
	}

	if(all(input.localID.xy == 0))
	{
		uint minMaterialBatchIdx;
		uint maxMaterialBatchIdx;
		UnpackBatchIndices(tileValues[0][0], OUT minMaterialBatchIdx, OUT maxMaterialBatchIdx);

		u_materialDepthTilesTexture[input.groupID.xy] = uint2(minMaterialBatchIdx, maxMaterialBatchIdx);
	}
}
