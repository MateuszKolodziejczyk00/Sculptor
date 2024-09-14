#include "SculptorShader.hlsli"

[[descriptor_set(ApplyInvalidationMaskDS, 0)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/MortonCode.hlsli"


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

	const uint2 invalidationMaskCoords = coords / uint2(8u, 4u);

	const uint invalidationMask = u_invalidationMask.Load(uint3(invalidationMaskCoords, 0u)).x;

	const uint2 tileLocalCoords = coords & uint2(7u, 3u);

	const uint bitIndex = EncodeMorton2(tileLocalCoords);

	if((invalidationMask >> bitIndex) & 1u)
	{
		const uint reservoirIdx =  GetScreenReservoirIdx(coords.xy, u_resamplingConstants.reservoirsResolution);

		uint packedProps = u_inOutReservoirsBuffer[reservoirIdx].MAndProps;
		packedProps = ModifyPackedReservoirM(packedProps, 0u);
		packedProps = ModifyPackedSpatialResamplingRangeID(packedProps, -3);
		u_inOutReservoirsBuffer[reservoirIdx].MAndProps = packedProps;
	}
}
