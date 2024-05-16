#include "SculptorShader.hlsli"

#ifndef GROUP_SIZE_Y
#error "GROUP_SIZE_Y must be defined"
#endif // GROUP_SIZE_Y


#ifndef GROUP_SIZE_X
#error "GROUP_SIZE_X must be defined"
#endif // GROUP_SIZE_X


[[descriptor_set(DDGIInvalidateProbesDS, 0)]]

#include "DDGI/DDGItypes.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void DDGIInvalidateProbesCS(CS_INPUT input)
{
	const uint3 updatedProbeCoords = input.groupID;
	
	const float3 probeWorldLocation = GetProbeWorldLocation(u_volumeParams, updatedProbeCoords);

	if (any(probeWorldLocation < u_invalidateParams.prevAABBMin) || any(probeWorldLocation > u_invalidateParams.prevAABBMax))
	{
		const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(u_volumeParams, updatedProbeCoords);

		const DDGIProbeDataCoords probeHitDistanceDataCoords = ComputeProbeHitDistanceDataOffset(u_volumeParams, probeWrappedCoords);
		const DDGIProbeDataCoords probeIlluminanceDataCoords = ComputeProbeIlluminanceDataOffset(u_volumeParams, probeWrappedCoords);

		const RWTexture2D<float4> hitDistanceTexture = u_volumeHitDistanceTextures[probeHitDistanceDataCoords.textureIdx];
		const RWTexture2D<float4> illuminanceTexture = u_volumeIlluminanceTextures[probeIlluminanceDataCoords.textureIdx];
		const RWTexture3D<float3> averageLuminanceTexture = u_volumeProbesAverageLuminanceTexture;

		hitDistanceTexture[probeHitDistanceDataCoords.textureLocalCoords + input.localID.xy] = -1.f;

		if(all(input.localID.xy < u_volumeParams.probeIlluminanceDataWithBorderRes))
		{
			illuminanceTexture[probeIlluminanceDataCoords.textureLocalCoords + input.localID.xy] = -1.f;
		}

		if(all(input.localID.xy == 0))
		{
			averageLuminanceTexture[probeWrappedCoords] = -1.f;
		}
	}
}
