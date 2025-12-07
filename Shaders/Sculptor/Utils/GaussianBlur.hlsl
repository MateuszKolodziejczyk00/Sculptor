#include "SculptorShader.hlsli"

[[descriptor_set(GaussianBlurDS, 0)]]


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


#define BLUR_MAX_KERNEL 32 

#define GROUP_SIZE 128

#define SHARED_DATA_SIZE (GROUP_SIZE + 2 * BLUR_MAX_KERNEL)


groupshared float4 sharedData[SHARED_DATA_SIZE];


[numthreads(GROUP_SIZE, 1, 1)]
void GaussianBlurCS(CS_INPUT input)
{
	int3 groupSize = 1;
	groupSize[u_constants.dimention] = GROUP_SIZE;

	int3 localID = 0;
	localID[u_constants.dimention] = input.localID.x;

	const int3 groupBeginPixel = input.groupID * groupSize;

	const int3 pixel = groupBeginPixel + localID;

	int3 offset = 0;
	offset[u_constants.dimention] = 1;

	const int kernelSize = min(u_constants.kernelSize, BLUR_MAX_KERNEL);

	const int3 maxPixel = u_constants.resolution - 1;

	const uint samplesNum = GROUP_SIZE + 2 * kernelSize;
	for (int i = input.localID.x; i < samplesNum; i += GROUP_SIZE)
	{
		const int3 samplePixel = clamp(groupBeginPixel + (i - kernelSize) * offset, 0, maxPixel);

		float4 sampleValue = 0.f;
		if(u_constants.is3DTexture)
		{
			sampleValue = u_input3D.Load(int4(samplePixel, 0));
		}
		else
		{
			sampleValue = u_input2D.Load(int3(samplePixel.xy, 0));
		}

		if (u_constants.useTonemappedValues)
		{
			sampleValue = (sampleValue / (sampleValue + 1.0f));
		}

		sharedData[i] = sampleValue;
	}

	GroupMemoryBarrierWithGroupSync();

	if(all(pixel < u_constants.resolution))
	{
		float4 inputSum = 0.f;
		float blurWeightSum = 0.f;
		
		for (int i = 0; i <= 2 * kernelSize; ++i)
		{
			const float4 inputValue = sharedData[input.localID.x + i];
			const float gaussianWeight = GaussianBlurWeight(abs(i - kernelSize), u_constants.sigma);
			inputSum  += inputValue * gaussianWeight;
			blurWeightSum += gaussianWeight;
		}

		float4 newValue = inputSum / (blurWeightSum + 0.0001f);

		if (u_constants.useTonemappedValues)
		{
			newValue = newValue / (1.0f - newValue + 0.0001f);
		}

		if(u_constants.is3DTexture)
		{
			u_output3D[pixel] = newValue;
		}
		else
		{
			u_output2D[pixel.xy] = newValue;
		}
	}
}
