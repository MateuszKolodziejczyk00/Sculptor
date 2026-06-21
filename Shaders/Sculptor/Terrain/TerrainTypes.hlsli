#ifndef TERRAIN_TYPES_HLSLI
#define TERRAIN_TYPES_HLSLI

struct TerrainMaterialsFactors
{
	uint4  materialIDs;
	float4 materialWeights;
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

	static TerrainDetilingSampler Initialize(in float2 uv)
	{
		TerrainDetilingSampler sampler;

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


[[shader_struct(TerrainMaterialsMap)]]

#ifndef SPT_TERRAIN_MATERIALS_BICUBIC_SAMPLING
#define SPT_TERRAIN_MATERIALS_BICUBIC_SAMPLING 1
#endif


TerrainMaterialsFactors CreateEmptyTerrainMaterialsFactors()
{
	TerrainMaterialsFactors factors;
	factors.materialIDs     = uint4(IDX_NONE_8, IDX_NONE_8, IDX_NONE_8, IDX_NONE_8);
	factors.materialWeights = 0.f;
	return factors;
}


void AccumulateTerrainMaterial(inout TerrainMaterialsFactors factors, uint materialID, float weight)
{
	if (weight <= 0.f || materialID == IDX_NONE_8)
	{
		return;
	}

	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		if (factors.materialIDs[idx] == materialID)
		{
			factors.materialWeights[idx] += weight;
			return;
		}
	}

	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		if (factors.materialIDs[idx] == IDX_NONE_8)
		{
			factors.materialIDs[idx]     = materialID;
			factors.materialWeights[idx] = weight;
			return;
		}
	}

	uint smallestWeightIdx = 0u;
	[unroll]
	for (uint idx = 1u; idx < 4u; ++idx)
	{
		if (factors.materialWeights[idx] < factors.materialWeights[smallestWeightIdx])
		{
			smallestWeightIdx = idx;
		}
	}

	if (weight > factors.materialWeights[smallestWeightIdx])
	{
		factors.materialIDs[smallestWeightIdx]     = materialID;
		factors.materialWeights[smallestWeightIdx] = weight;
	}
}


void AccumulateTerrainMaterialsGather(inout TerrainMaterialsFactors factors, uint4 materialIDs, float4 materialWeights)
{
	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		AccumulateTerrainMaterial(factors, materialIDs[idx], materialWeights[idx]);
	}
}


void NormalizeTerrainMaterialsFactors(inout TerrainMaterialsFactors factors)
{
	float totalWeight = 0.f;

	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		totalWeight += factors.materialWeights[idx];
	}

	if (totalWeight > 0.f)
	{
		factors.materialWeights *= rcp(totalWeight);
	}
}


float CubicBSplineWeight(float x)
{
	x = abs(x);
	if (x < 1.f)
	{
		return (4.f - 6.f * x * x + 3.f * x * x * x) / 6.f;
	}
	else if (x < 2.f)
	{
		const float t = 2.f - x;
		return (t * t * t) / 6.f;
	}
	else
	{
		return 0.f;
	}
}


TerrainMaterialsFactors SampleTerrainMaterialsMap(in TerrainMaterialsMap materialsMap, in float2 worldLocation)
{
	const float2 uv = saturate((worldLocation - materialsMap.minBounds) * materialsMap.rcpBoundsSize);

#if SPT_TERRAIN_MATERIALS_BICUBIC_SAMPLING
	const float2 bilinearCoords = uv * materialsMap.resolution - 0.5f + SPT_SUB_PIXEL_PRECITION_OFFSET;
	const float2 bilinearFrac   = frac(bilinearCoords);
	const float2 texelSize      = materialsMap.rcpResolution;

	const float4 weightsX = float4(CubicBSplineWeight(-1.f - bilinearFrac.x),
								   CubicBSplineWeight(     - bilinearFrac.x),
								   CubicBSplineWeight( 1.f - bilinearFrac.x),
								   CubicBSplineWeight( 2.f - bilinearFrac.x));

	const float4 weightsY = float4(CubicBSplineWeight(-1.f - bilinearFrac.y),
								   CubicBSplineWeight(     - bilinearFrac.y),
								   CubicBSplineWeight( 1.f - bilinearFrac.y),
								   CubicBSplineWeight( 2.f - bilinearFrac.y));

	const uint4 gatherLowerLeft  = materialsMap.materialIDs.Gather<uint4>(BindlessSamplers::NearestClampEdge(), uv + float2(-texelSize.x, -texelSize.y));
	const uint4 gatherLowerRight = materialsMap.materialIDs.Gather<uint4>(BindlessSamplers::NearestClampEdge(), uv + float2( texelSize.x, -texelSize.y));
	const uint4 gatherUpperLeft  = materialsMap.materialIDs.Gather<uint4>(BindlessSamplers::NearestClampEdge(), uv + float2(-texelSize.x,  texelSize.y));
	const uint4 gatherUpperRight = materialsMap.materialIDs.Gather<uint4>(BindlessSamplers::NearestClampEdge(), uv + float2( texelSize.x,  texelSize.y));

	TerrainMaterialsFactors factors = CreateEmptyTerrainMaterialsFactors();

	AccumulateTerrainMaterialsGather(factors, gatherLowerLeft, float4(weightsX.x * weightsY.y,
																	  weightsX.y * weightsY.y,
																	  weightsX.y * weightsY.x,
																	  weightsX.x * weightsY.x));

	AccumulateTerrainMaterialsGather(factors, gatherLowerRight, float4(weightsX.z * weightsY.y,
																	   weightsX.w * weightsY.y,
																	   weightsX.w * weightsY.x,
																	   weightsX.z * weightsY.x));

	AccumulateTerrainMaterialsGather(factors, gatherUpperLeft, float4(weightsX.x * weightsY.w,
																	  weightsX.y * weightsY.w,
																	  weightsX.y * weightsY.z,
																	  weightsX.x * weightsY.z));

	AccumulateTerrainMaterialsGather(factors, gatherUpperRight, float4(weightsX.z * weightsY.w,
																	   weightsX.w * weightsY.w,
																	   weightsX.w * weightsY.z,
																	   weightsX.z * weightsY.z));

	NormalizeTerrainMaterialsFactors(factors);

	return factors;
#else
	const float2 bilinearCoords  = uv * materialsMap.resolution - 0.5f + SPT_SUB_PIXEL_PRECITION_OFFSET;

	const uint4 materialIDs = materialsMap.materialIDs.Gather<uint4>(BindlessSamplers::NearestClampEdge(), uv);

	const float2 bilinearFrac    = frac(bilinearCoords);
	const float2 invBilinearFrac = 1.f - bilinearFrac;

	float4 materialWeights       = float4(invBilinearFrac.x * bilinearFrac.y,
										  bilinearFrac.x * bilinearFrac.y,
										  bilinearFrac.x * invBilinearFrac.y,
										  invBilinearFrac.x * invBilinearFrac.y);

	[unroll]
	for (uint materialIdx = 0u; materialIdx < 4u; ++materialIdx)
	{
		[unroll]
		for (uint prevMaterialIdx = 0u; prevMaterialIdx < materialIdx; ++prevMaterialIdx)
		{
			if (materialIDs[materialIdx] == materialIDs[prevMaterialIdx])
			{
				materialWeights[prevMaterialIdx] += materialWeights[materialIdx];
				materialWeights[materialIdx] = 0.f;
				break;
			}
		}
	}

	TerrainMaterialsFactors factors;
	factors.materialIDs     = materialIDs;
	factors.materialWeights = materialWeights;

	return factors;
#endif
}

#endif // TERRAIN_TYPES_HLSLI
