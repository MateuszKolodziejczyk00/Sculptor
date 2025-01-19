#include "SculptorShader.hlsli"

[[descriptor_set(ApplyInvalidationMaskDS, 0)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/MortonCode.hlsli"

#include "Utils/VariableRate/VariableRate.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 localID  : SV_GroupThreadID;
};


#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void ApplyInvalidationMaskCS(CS_INPUT input)
{
	const uint2 coords = min(input.globalID.xy, u_resamplingConstants.resolution - 1u);

	const uint2 maskTileCoords = coords / uint2(8u, 4u);
	const uint2 tileLocalCoords = coords & uint2(7u, 3u);

	const uint invalidationMask = u_invalidationMask.Load(uint3(maskTileCoords, 0u)).x;

	const uint bitIndex = EncodeMorton2(tileLocalCoords);
	const bool isInvalidated = ((invalidationMask >> bitIndex) & 1u);

	if (isInvalidated)
	{
		const uint reservoirIdx = GetScreenReservoirIdx(coords, u_resamplingConstants.reservoirsResolution);

		SRPackedReservoir initialReservoir = u_initialReservoirsBuffer[reservoirIdx];
		uint packedProps = initialReservoir.MAndProps;
		packedProps = ModifyPackedSpatialResamplingRangeID(packedProps, -7);
		initialReservoir.MAndProps = packedProps;

		u_inOutReservoirsBuffer[reservoirIdx] = initialReservoir;
	}
}
