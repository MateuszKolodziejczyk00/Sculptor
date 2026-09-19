#include "SculptorShader.hlsli"

[[shader_params(GaussianBlurConstants, PARAMS_GAUSSIAN_BLUR)]]


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
	groupSize[PARAMS_GAUSSIAN_BLUR->dimention] = GROUP_SIZE;

	int3 localID = 0;
	localID[PARAMS_GAUSSIAN_BLUR->dimention] = input.localID.x;

	const int3 groupBeginPixel = input.groupID * groupSize;

	const int3 pixel = groupBeginPixel + localID;

	int3 offset = 0;
	offset[PARAMS_GAUSSIAN_BLUR->dimention] = 1;

	const int kernelSize = min(PARAMS_GAUSSIAN_BLUR->kernelSize, BLUR_MAX_KERNEL);

	const int3 maxPixel = PARAMS_GAUSSIAN_BLUR->resolution - 1;

	const uint samplesNum = GROUP_SIZE + 2 * kernelSize;
	for (int i = input.localID.x; i < samplesNum; i += GROUP_SIZE)
	{
		const int3 samplePixel = clamp(groupBeginPixel + (i - kernelSize) * offset, 0, maxPixel);

		float4 sampleValue = 0.f;
		if(PARAMS_GAUSSIAN_BLUR->is3DTexture)
		{
			sampleValue = PARAMS_GAUSSIAN_BLUR->input3D.Load(int4(samplePixel, 0));
		}
		else
		{
			sampleValue = PARAMS_GAUSSIAN_BLUR->input2D.Load(int3(samplePixel.xy, 0));
		}

		if (PARAMS_GAUSSIAN_BLUR->useTonemappedValues)
		{
			sampleValue = (sampleValue / (sampleValue + 1.0f));
		}

		sharedData[i] = sampleValue;
	}

	GroupMemoryBarrierWithGroupSync();

	if(all(pixel < PARAMS_GAUSSIAN_BLUR->resolution))
	{
		float4 inputSum = 0.f;
		float blurWeightSum = 0.f;
		
		for (int i = 0; i <= 2 * kernelSize; ++i)
		{
			const float4 inputValue = sharedData[input.localID.x + i];
			const float gaussianWeight = GaussianBlurWeight(abs(i - kernelSize), PARAMS_GAUSSIAN_BLUR->sigma);
			inputSum  += inputValue * gaussianWeight;
			blurWeightSum += gaussianWeight;
		}

		float4 newValue = inputSum / (blurWeightSum + 0.0001f);

		if (PARAMS_GAUSSIAN_BLUR->useTonemappedValues)
		{
			newValue = newValue / (1.0f - newValue + 0.0001f);
		}

		if(PARAMS_GAUSSIAN_BLUR->is3DTexture)
		{
			PARAMS_GAUSSIAN_BLUR->output3D[pixel] = newValue;
		}
		else
		{
			PARAMS_GAUSSIAN_BLUR->output2D[pixel.xy] = newValue;
		}
	}
}
