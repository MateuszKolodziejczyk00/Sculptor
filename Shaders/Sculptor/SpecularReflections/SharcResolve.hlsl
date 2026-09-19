#include "SculptorShader.hlsli"

[[shader_params(SharcResolveParams, PARAMS_SHARC_RESOLVE)]]
[[shader_params(GPURenderView, VIEW)]]


#include "SpecularReflections/SculptorSharc.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void SharcResolveCS(CS_INPUT input)
{
	SharcDef sharcDef;
    sharcDef.cameraPosition = PARAMS_SHARC_RESOLVE->sharcConstants.viewLocation;
    sharcDef.capacity       = PARAMS_SHARC_RESOLVE->sharcConstants.sharcCapacity;
    sharcDef.hashEntries    = PARAMS_SHARC_RESOLVE->hashEntries.GetResource();
    sharcDef.voxelData      = PARAMS_SHARC_RESOLVE->voxelData.GetResource();
    sharcDef.voxelDataPrev  = PARAMS_SHARC_RESOLVE->voxelDataPrev.GetResource();
    sharcDef.exposure       = VIEW->viewExposure->exposure;

    SharcParameters sharcParams = CreateSharcParameters(sharcDef);

    SharcResolveParameters resolveParams;
    resolveParams.cameraPositionPrev      = PARAMS_SHARC_RESOLVE->sharcConstants.prevViewLocation;
    resolveParams.accumulationFrameNum    = 32u;
    resolveParams.staleFrameNumMax        = 64u;
    resolveParams.enableAntiFireflyFilter = true;
    resolveParams.exposurePrev            = VIEW->viewExposure->exposureLastFrame;
    SharcResolveEntry(input.globalID.x, sharcParams, resolveParams);
}
