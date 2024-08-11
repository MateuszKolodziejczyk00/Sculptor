#include "SculptorShader.hlsli"

[[descriptor_set(VRTVisibilityResolveDS, 0)]]

#include "Utils/VariableRate/VariableRate.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float LoadTraceVRBlockVisibility(in uint2 blockCoords)
{
	const uint vrBlockInfo = u_vrBlocksTexture.Load(uint3(blockCoords, 0)).x;

	uint2 localTraceOffset = 0;
	uint vrMask = 0;
	UnpackVRBlockInfo(vrBlockInfo, OUT localTraceOffset, OUT vrMask);

	blockCoords &= ~(GetVariableRateTileSize(vrMask) - 1);
	const uint2 traceCoods = blockCoords + localTraceOffset;

	return u_inputTexture.Load(uint3(traceCoods, 0)).x;
}


class VariableRateSolver
{
	static VariableRateSolver Create(in float visibilitySample)
	{
		VariableRateSolver solver;
		solver.m_sampleVisibility  = visibilitySample;
		solver.m_samples           = 1.f;
		return solver;
	}

	void AccumulateBlock(in float visibilitySample)
	{
		m_sampleVisibility += visibilitySample;
		m_samples += 1.f;
	}

	float GetVisibility()
	{
		const float visibility = m_samples > 0.f ? m_sampleVisibility / m_samples : 0.f;;

		// Denoiser expects visibility to be either 0 or 1
		// This also can help a bit with flickering
		return visibility > 0.49f ? 1.f : 0.f;
	}

	float m_sampleVisibility;
	float m_samples;
};


[numthreads(8, 4, 1)]
void ResolveVRTVisibilityCS(CS_INPUT input)
{
	if(all(input.globalID.xy < u_constants.resolution))
	{
		const uint2 pixelCoords = input.globalID.xy;

		const uint vrBlockInfo = u_vrBlocksTexture.Load(uint3(pixelCoords, 0)).x;

		uint2 localTraceOffset = 0;
		uint vrMask = 0;
		UnpackVRBlockInfo(vrBlockInfo, OUT localTraceOffset, OUT vrMask);
		const uint2 blockSize = GetVariableRateTileSize(vrMask);
		const uint2 blockCoords = uint2(pixelCoords.x & ~(blockSize.x - 1), pixelCoords.y & ~(blockSize.y - 1));

		const uint2 traceCoords = blockCoords + localTraceOffset;

		const float traceVisibility = u_inputTexture.Load(uint3(traceCoords, 0)).x;

		VariableRateSolver vrSolver = VariableRateSolver::Create(traceVisibility);

		const uint2 offsetFromTrace = pixelCoords - traceCoords;

		const uint otherBlockXCoord = blockCoords.x + (offsetFromTrace.x > 0 ? blockSize.x : -1);
		const uint otherBlockYCoord = blockCoords.y + (offsetFromTrace.y > 0 ? blockSize.y : -1);

		if(offsetFromTrace.x != 0)
		{
			vrSolver.AccumulateBlock(LoadTraceVRBlockVisibility(uint2(otherBlockXCoord, blockCoords.y)));
		}

		if(offsetFromTrace.y != 0)
		{
			vrSolver.AccumulateBlock(LoadTraceVRBlockVisibility(uint2(blockCoords.x, otherBlockYCoord)));
		}

		if(offsetFromTrace.x != 0 && offsetFromTrace.y != 0)
		{
			vrSolver.AccumulateBlock(LoadTraceVRBlockVisibility(uint2(otherBlockXCoord, otherBlockYCoord)));
		}

		u_outputTexture[pixelCoords] = vrSolver.GetVisibility();
	}
}