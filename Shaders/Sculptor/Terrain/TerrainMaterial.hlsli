#ifndef TERRAIN_MATERIAL_HLSLI
#define TERRAIN_MATERIAL_HLSLI

#include "Materials/MaterialSystem.hlsli"
#include "Terrain/TerrainTypes.hlsli"


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


struct TerrainMaterialEvaluationOutput
{
	MaterialEvaluationOutput material;
	float                    pomDepth;
};


TerrainMaterialEvaluationOutput EvaluateTerrainMaterial(in MaterialUnifiedData materialsData, in MaterialEvaluationParameters evalParams, in TerrainMaterialData terrainMaterials, in TerrainMaterialsFactors materialsFactors)
{
	TerrainMaterialEvaluationOutput output = (TerrainMaterialEvaluationOutput)0.f;
	float totalWeight = 0.f;

	const float2 uv = evalParams.uv;

	for (int i = 0; i < 4; ++i)
	{
		const uint materialID = materialsFactors.materialIDs[i];
		const float weight = materialsFactors.materialWeights[i];

		if (weight > 0.f)
		{
			const TerrainMaterialEntry materialEntry = terrainMaterials.terrainMaterials[materialID];

			const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData<SPT_MATERIAL_DATA_TYPE>(materialsData, materialEntry.dataHandle);

			evalParams.uv = uv * materialEntry.uvScale;

			TerrainDetilingSampler sampler = TerrainDetilingSampler::Initialize(evalParams);
			const MaterialEvaluationOutput materialEvalOutput = EvaluateMaterial(sampler, evalParams, materialData);

			output.material.baseColor      += materialEvalOutput.baseColor * weight;
			output.material.roughness      += materialEvalOutput.roughness * weight;
			output.material.metallic       += materialEvalOutput.metallic * weight;
			output.material.occlusion      += materialEvalOutput.occlusion * weight;
			output.material.shadingNormal  += materialEvalOutput.shadingNormal * weight;
			output.material.geometryNormal += materialEvalOutput.geometryNormal  * weight;

#if defined(MATERIAL_DEPTH_DATA_ACCESSOR)
			MaterialDepthData materialDepthData = materialData.MATERIAL_DEPTH_DATA_ACCESSOR;
			output.pomDepth += sampler.Sample(materialDepthData.depthTexture, BindlessSamplers::MaterialAniso(), evalParams.uv);
#endif // MATERIAL_DEPTH_DATA_ACCESSOR

			totalWeight += weight;
		}
	}

	const float rcpTotalWeight = 1.f / totalWeight;

	if (totalWeight > 0.f)
	{
		output.material.baseColor      *= rcpTotalWeight;
		output.material.roughness      *= rcpTotalWeight;
		output.material.metallic       *= rcpTotalWeight;
		output.material.occlusion      *= rcpTotalWeight;
		output.material.shadingNormal  = normalize(output.material.shadingNormal * rcpTotalWeight);
		output.material.geometryNormal = normalize(output.material.geometryNormal * rcpTotalWeight);

		output.pomDepth *= rcpTotalWeight;
	}

	return output;
}


#ifdef DS_RenderSceneDS
TerrainMaterialEvaluationOutput EvaluateTerrainMaterial(in MaterialEvaluationParameters evalParams, in TerrainMaterialData terrainMaterials, in TerrainMaterialsFactors materialsFactors)
{
	return EvaluateTerrainMaterial(GPUMaterials().data, evalParams, terrainMaterials, materialsFactors);
}
#endif // DS_RenderSceneDS

#endif // TERRAIN_MATERIAL_HLSLI
