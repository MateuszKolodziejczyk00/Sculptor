#include "SculptorShader.hlsli"

[[descriptor_set(GenerateOctahedronNormalsDS, 0)]]

#include "Utils/GBuffer/GBuffer.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void GenerateOctahedronNormalsCS(CS_INPUT input)
{
	const uint2 coords = min(input.globalID.xy, u_constants.resolution - 1u);

	const float4 packedTangentFrame = u_tangentFrame.Load(uint3(coords, 0u));
	const float3 normal = DecodeGBufferNormal(packedTangentFrame);

	const float2 octahedronNormal = OctahedronEncodeNormal(normal);

	u_rwOctahedronNormals[coords] = octahedronNormal;
}
