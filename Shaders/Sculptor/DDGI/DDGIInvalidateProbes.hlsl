#include "SculptorShader.hlsli"

#ifndef GROUP_SIZE_Y
#error "GROUP_SIZE_Y must be defined"
#endif // GROUP_SIZE_Y


#ifndef GROUP_SIZE_X
#error "GROUP_SIZE_X must be defined"
#endif // GROUP_SIZE_X


[[shader_params(DDGIInvalidateProbesConsts, CONSTS)]]

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
	
	const float3 probeWorldLocation = GetProbeWorldLocation(*CONSTS->volumeParams, updatedProbeCoords);

	if (CONSTS->forceInvalidateAll || any(probeWorldLocation < CONSTS->prevAABBMin - 0.01f) || any(probeWorldLocation > CONSTS->prevAABBMax + 0.01f))
	{
		const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(*CONSTS->volumeParams, updatedProbeCoords);

		const DDGIProbeDataCoords probeHitDistanceDataCoords = ComputeProbeHitDistanceDataOffset(*CONSTS->volumeParams, probeWrappedCoords);
		const DDGIProbeDataCoords probeIlluminanceDataCoords = ComputeProbeIlluminanceDataOffset(*CONSTS->volumeParams, probeWrappedCoords);

		UAVTexture2D<float4> hitDistanceTexture = CONSTS->volumeHitDistanceTextures[probeHitDistanceDataCoords.textureIdx];
		UAVTexture2D<float4> illuminanceTexture = CONSTS->volumeIlluminanceTextures[probeIlluminanceDataCoords.textureIdx];
		UAVTexture3D<float4> averageLuminanceTexture = CONSTS->volumeProbesAverageLuminanceTexture;

		if(all(input.localID.xy < CONSTS->volumeParams->probeHitDistanceDataWithBorderRes))
		{
			hitDistanceTexture[probeHitDistanceDataCoords.textureLocalCoords + input.localID.xy] = -1.f;
		}

		if(all(input.localID.xy < CONSTS->volumeParams->probeIlluminanceDataWithBorderRes))
		{
			illuminanceTexture[probeIlluminanceDataCoords.textureLocalCoords + input.localID.xy] = 0.f;
		}

		if(all(input.localID.xy == 0))
		{
			averageLuminanceTexture[probeWrappedCoords] = -1.f;
		}
	}
}
