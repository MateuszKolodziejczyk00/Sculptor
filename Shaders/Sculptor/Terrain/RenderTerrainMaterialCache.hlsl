#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]

[[shader_params(UpdateMaterialCacheConstants, u_constants)]]

#define SPT_MATERIAL_DATA_TYPE    MaterialPBRData
#define SPT_MATERIAL_SHADER_PATH "Sculptor/Materials/DefaultPBR.hlsli"

#define FULLSCREEN_FORCE_DEPTH 0.5f

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/FullScreen.hlsli"
#include "Utils/Packing.hlsli"
#include "Hashing.hlsli"
#include "Materials/MaterialSystem.hlsli"
#include "Terrain/SceneTerrain.hlsli"


struct PS_OUTPUT
{
	float4 baseColorMetallic  : SV_Target0;
	float2 normals            : SV_Target1;
	float2 roughnessOcclusion : SV_Target2;
#if RENDER_POM_DEPTH
	float pomDepth            : SV_Target3;
#endif // RENDER_POM_DEPTH
};


struct TerrainDetilingSampler
{
	float2 uva;
	float2 uvb;
	float2 uvc;
	float2 uvd;

	float2 ddxa;
	float2 ddxb;
	float2 ddxc;
	float2 ddxd;

	float2 ddya;
	float2 ddyb;
	float2 ddyc;
	float2 ddyd;

	static float4 Hash4(float2 p)
	{
		return frac(sin(float4(1.0 + dot(p, float2(37.0, 17.0)),
							   2.0 + dot(p, float2(11.0, 47.0)),
							   3.0 + dot(p, float2(41.0, 29.0)),
							   4.0 + dot(p, float2(23.0, 31.0)))) * 103.0);
	}

	static TerrainDetilingSampler Initialize(MaterialEvaluationParameters evalParams)
	{
		TerrainDetilingSampler sampler;

		const float2 uv = evalParams.uv;

		const float2 iuv = floor(uv);
		const float2 fuv = frac(uv);

		// generate per-tile transform
		float4 ofa = Hash4(iuv + float2(0.0, 0.0));
		float4 ofb = Hash4(iuv + float2(1.0, 0.0));
		float4 ofc = Hash4(iuv + float2(0.0, 1.0));
		float4 ofd = Hash4(iuv + float2(1.0, 1.0));
		
		const float2 dx = ddx(uv);
		const float2 dy = ddy(uv);
	
		// transform per-tile uvs
		ofa.zw = sign(ofa.zw-0.5);
		ofb.zw = sign(ofb.zw-0.5);
		ofc.zw = sign(ofc.zw-0.5);
		ofd.zw = sign(ofd.zw-0.5);
		
		// uv's, and derivarives (for correct mipmapping)
		sampler.uva = uv * ofa.zw + ofa.xy;
		sampler.uvb = uv * ofb.zw + ofb.xy;
		sampler.uvc = uv * ofc.zw + ofc.xy;
		sampler.uvd = uv * ofd.zw + ofd.xy;

		sampler.ddxa = dx * ofa.zw;
		sampler.ddxb = dx * ofb.zw;
		sampler.ddxc = dx * ofc.zw;
		sampler.ddxd = dx * ofd.zw;

		sampler.ddya = dy * ofa.zw;
		sampler.ddyb = dy * ofb.zw;
		sampler.ddyc = dy * ofc.zw;
		sampler.ddyd = dy * ofd.zw;

		return sampler;
	}

	template<typename T>
	T Sample(in SRVTexture2D<T> texture, SamplerState sampler, in float2 uv)
	{
		const float2 b = smoothstep(0.25,0.75, frac(uv));
		
		const T sampleA = texture.Sample(sampler, uva);
		const T sampleB = texture.Sample(sampler, uvb);
		const T sampleC = texture.Sample(sampler, uvc);
		const T sampleD = texture.Sample(sampler, uvd);

		 return lerp(lerp(sampleA, sampleB, b.x), 
					 lerp(sampleC, sampleD, b.x), b.y);
	}
};


PS_OUTPUT RenderTerrainMaterialCacheFS(VS_OUTPUT input)
{
	TerrainInterface terrainInterface = SceneTerrain();

	const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(u_constants.terrainMaterial.terrainMaterial);

	const float2 lodMinUV = u_constants.minBounds / u_constants.range;
	const float2 lodUV = frac(input.uv - lodMinUV);

	float3 worldLocation;
	worldLocation.xy = u_constants.minBounds + lodUV * u_constants.range;
	worldLocation.z  = terrainInterface.GetHeight(worldLocation.xy);

	const float3 normal = terrainInterface.GetNormal(worldLocation.xy);
	const float3 tangent = terrainInterface.GetTangent(worldLocation.xy);
	const float3 bitangent = terrainInterface.GetBitangent(worldLocation.xy);

	const float2 uvScale = 1.f / 2.1f;
	const float2 uv = worldLocation.xy * uvScale;

	MaterialEvaluationParameters evalParams;
	evalParams.normal        = normal;
	evalParams.tangent       = tangent;
	evalParams.bitangent     = bitangent;
	evalParams.hasTangent    = true;
	evalParams.uv            = uv;
	evalParams.worldLocation = worldLocation;
	evalParams.clipSpace     = 0.f;

	TerrainDetilingSampler sampler = TerrainDetilingSampler::Initialize(evalParams);
	const MaterialEvaluationOutput materialEvalOutput = EvaluateMaterial(sampler, evalParams, materialData);

#if RENDER_POM_DEPTH
	float pomDepth = 0.f;
#if defined(MATERIAL_DEPTH_DATA_ACCESSOR)
	MaterialDepthData materialDepthData = materialData.MATERIAL_DEPTH_DATA_ACCESSOR;
	pomDepth = materialDepthData.depthTexture.SampleLevel(BindlessSamplers::MaterialAniso(), uv, 0.f);
#endif // MATERIAL_DEPTH_DATA_ACCESSOR
#endif // RENDER_POM_DEPTH

	const float2 encodedNormal = OctahedronEncodeNormal(materialEvalOutput.shadingNormal);

	PS_OUTPUT output;
	output.baseColorMetallic  = float4(materialEvalOutput.baseColor, materialEvalOutput.metallic);
	output.normals            = encodedNormal;
	output.roughnessOcclusion = float2(materialEvalOutput.roughness, materialEvalOutput.occlusion);
#if RENDER_POM_DEPTH
	output.pomDepth = pomDepth;
#endif // RENDER_POM_DEPTH

	return output;
}
