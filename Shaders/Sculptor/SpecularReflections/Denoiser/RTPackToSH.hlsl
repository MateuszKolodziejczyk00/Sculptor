#include "SculptorShader.hlsli"

[[descriptor_set(RTPackToSHDS, 0)]]

#include "Utils/SphericalHarmonics.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void RTPackToSHCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;

	const float3 normal = OctahedronDecodeNormal(u_constants.normals.Load(coords));
	const float3 specular = u_constants.specular.Load(coords);
	const float3 diffuse = u_constants.diffuse.Load(coords);

	const float3 specularYCoCg = RGBToYCoCg(specular);
	const float3 diffuseYCoCg = RGBToYCoCg(diffuse);

	const SH2<float> specularY_SH2 = CreateSH2<float>(specularYCoCg.x, normal);
	const SH2<float> diffuseY_SH2  = CreateSH2<float>(diffuseYCoCg.x, normal);

	const float4 diffSpecCoCg = float4(diffuseYCoCg.yz, specularYCoCg.yz);

	u_constants.rwSpecularY.Store(coords, SH2ToFloat4(specularY_SH2));
	u_constants.rwDiffuseY.Store(coords, SH2ToFloat4(diffuseY_SH2));
	u_constants.rwDiffSpecCoCg.Store(coords, diffSpecCoCg);
}
