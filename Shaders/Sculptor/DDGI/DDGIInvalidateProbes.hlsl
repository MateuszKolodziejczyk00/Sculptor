#include "SculptorShader.hlsli"

#ifndef GROUP_SIZE_Y
#error "GROUP_SIZE_Y must be defined"
#endif // GROUP_SIZE_Y


#ifndef GROUP_SIZE_X
#error "GROUP_SIZE_X must be defined"
#endif // GROUP_SIZE_X


[[shader_params(DDGIInvalidateProbesConsts, PARAMS_D_D_G_I_INVALIDATE_PROBES)]]

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
	
	const float3 probeWorldLocation = GetProbeWorldLocation(*PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeParams, updatedProbeCoords);

	if (PARAMS_D_D_G_I_INVALIDATE_PROBES->forceInvalidateAll || any(probeWorldLocation < PARAMS_D_D_G_I_INVALIDATE_PROBES->prevAABBMin - 0.01f) || any(probeWorldLocation > PARAMS_D_D_G_I_INVALIDATE_PROBES->prevAABBMax + 0.01f))
	{
		const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(*PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeParams, updatedProbeCoords);

		const DDGIProbeDataCoords probeHitDistanceDataCoords = ComputeProbeHitDistanceDataOffset(*PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeParams, probeWrappedCoords);
		const DDGIProbeDataCoords probeIlluminanceDataCoords = ComputeProbeIlluminanceDataOffset(*PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeParams, probeWrappedCoords);

		UAVTexture2D<float4> hitDistanceTexture = PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeHitDistanceTextures[probeHitDistanceDataCoords.textureIdx];
		UAVTexture2D<float4> illuminanceTexture = PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeIlluminanceTextures[probeIlluminanceDataCoords.textureIdx];
		UAVTexture3D<float4> averageLuminanceTexture = PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeProbesAverageLuminanceTexture;

		if(all(input.localID.xy < PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeParams->probeHitDistanceDataWithBorderRes))
		{
			hitDistanceTexture[probeHitDistanceDataCoords.textureLocalCoords + input.localID.xy] = -1.f;
		}

		if(all(input.localID.xy < PARAMS_D_D_G_I_INVALIDATE_PROBES->volumeParams->probeIlluminanceDataWithBorderRes))
		{
			illuminanceTexture[probeIlluminanceDataCoords.textureLocalCoords + input.localID.xy] = 0.f;
		}

		if(all(input.localID.xy == 0))
		{
			averageLuminanceTexture[probeWrappedCoords] = -1.f;
		}
	}
}
