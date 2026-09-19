#include "SculptorShader.hlsli"

[[shader_params(RTCopyTracedReservoirsParams, PARAMS_R_T_COPY_TRACED_RESERVOIRS)]]

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void RTCopyTracedReservoirsCS(CS_INPUT input)
{
	const uint traceCommandIndex = input.globalID.x;

	if(traceCommandIndex >= PARAMS_R_T_COPY_TRACED_RESERVOIRS->tracesNum[0])
	{
		return;
	}

	const EncodedRayTraceCommand encodedTraceCommand = PARAMS_R_T_COPY_TRACED_RESERVOIRS->traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 coords = traceCommand.blockCoords + traceCommand.localOffset;

	const uint reservoirIdx = GetScreenReservoirIdx(coords, PARAMS_R_T_COPY_TRACED_RESERVOIRS->resamplingConstants->reservoirsResolution);

	PARAMS_R_T_COPY_TRACED_RESERVOIRS->outReservoirs[reservoirIdx] = PARAMS_R_T_COPY_TRACED_RESERVOIRS->inReservoirs[reservoirIdx];
}