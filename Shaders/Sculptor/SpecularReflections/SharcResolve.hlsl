#include "SculptorShader.hlsli"

[[descriptor_set(SharcResolveDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


#include "SpecularReflections/SculptorSharc.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void SharcResolveCS(CS_INPUT input)
{
	SharcDef sharcDef;
    sharcDef.cameraPosition = u_constants.viewLocation;
    sharcDef.capacity       = u_constants.sharcCapacity;
    sharcDef.hashEntries    = u_hashEntries;
    sharcDef.voxelData      = u_voxelData;
    sharcDef.voxelDataPrev  = u_voxelDataPrev;
    sharcDef.exposure       = u_viewExposure.exposure;

    SharcParameters sharcParams = CreateSharcParameters(sharcDef);

    SharcResolveParameters resolveParams;
    resolveParams.cameraPositionPrev      = u_constants.prevViewLocation;
    resolveParams.accumulationFrameNum    = 32u;
    resolveParams.staleFrameNumMax        = 64u;
    resolveParams.enableAntiFireflyFilter = true;
    resolveParams.exposurePrev            = u_viewExposure.exposureLastFrame;
    SharcResolveEntry(input.globalID.x, sharcParams, resolveParams);
}
