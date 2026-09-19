#include "SculptorShader.hlsli"

[[shader_params(RTPackToSHConstants, PARAMS_R_T_PACK_TO_S_H)]]

#include "Utils/Packing.hlsli"
#include "SpecularReflections/Denoiser/RTDenoising.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void RTPackToSHCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;

	const float3 normal = OctahedronDecodeNormal(PARAMS_R_T_PACK_TO_S_H->lightDirection.Load(coords));
	const float3 specular = PARAMS_R_T_PACK_TO_S_H->specular.Load(coords);
	const float3 diffuse = PARAMS_R_T_PACK_TO_S_H->diffuse.Load(coords);

	const float3 specularYCoCg = RGBToYCoCg(specular);
	const float3 diffuseYCoCg = RGBToYCoCg(diffuse);

	const RTSphericalBasis specularY_SH2 = CreateRTSphericalBasis(specularYCoCg.x, normal);
	const RTSphericalBasis diffuseY_SH2  = CreateRTSphericalBasis(diffuseYCoCg.x, normal);

	const float4 diffSpecCoCg = float4(diffuseYCoCg.yz, specularYCoCg.yz);

	PARAMS_R_T_PACK_TO_S_H->rwSpecularY.Store(coords, RTSphericalBasisToRaw(specularY_SH2));
	PARAMS_R_T_PACK_TO_S_H->rwDiffuseY.Store(coords, RTSphericalBasisToRaw(diffuseY_SH2));
	PARAMS_R_T_PACK_TO_S_H->rwDiffSpecCoCg.Store(coords, diffSpecCoCg);
}
