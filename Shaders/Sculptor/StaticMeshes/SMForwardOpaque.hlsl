#include "SculptorShader.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(RenderViewDS, 3)]]

[[descriptor_set(StaticMeshBatchDS, 4)]]
[[descriptor_set(SMIndirectRenderTrianglesDS, 5)]]

[[descriptor_set(MaterialsDS, 6)]]

[[descriptor_set(ViewShadingInputDS, 7)]]
[[descriptor_set(ShadowMapsDS, 8)]]

#ifdef ENABLE_DDGI

[[descriptor_set(DDGISceneDS, 9)]]

#include "DDGI/DDGITypes.hlsli"

#endif // ENABLE_DDGI

#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Lights/Lighting.hlsli"
#include "Utils/SceneViewUtils.hlsli"


#include "Materials/MaterialSystem.hlsli"
#include SPT_MATERIAL_SHADER_PATH


struct VS_INPUT
{
    uint index : SV_VertexID;
};


struct VS_OUTPUT
{
    float4  clipSpace           : SV_POSITION;

    float3  normal              : VERTEX_NORMAL;
    uint16_t    materialDataID  : MATERIAL_DATA;
    float3  tangent             : VERTEX_TANGENT;
    bool    hasTangent          : VERTEX_HAS_TANGENT;
    float3  bitangent           : VERTEX_BITANGENT;
    float2  uv                  : VERTEX_UV;
    
    float3  worldLocation       : WORLD_LOCATION;

#if WITH_DEBUGS
    uint    meshletIdx          : MESHLET_IDX;
#endif // WITH_DEBUGS

    float4  pixelClipSpace      : CLIP_SPACE;
};


VS_OUTPUT SMForwardOpaque_VS(VS_INPUT input)
{
    VS_OUTPUT output;

    const uint workloadIdx = input.index / 3;
    const uint triangleVertexIdx = input.index - workloadIdx * 3;

    uint batchElementIdx;
    uint localMeshletIdx;
    uint triangleIdx;
    UnpackTriangleWorkload(u_triangleWorkloads[workloadIdx], batchElementIdx, localMeshletIdx, triangleIdx);
    
    const RenderEntityGPUData entityData = u_renderEntitiesData[u_visibleBatchElements[batchElementIdx].entityIdx];
    const float4x4 instanceTransform = entityData.transform;

    const SubmeshGPUData submesh = u_submeshes[u_visibleBatchElements[batchElementIdx].submeshGlobalIdx];
    
    const uint meshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;
    const MeshletGPUData meshlet = u_meshlets[meshletIdx];

    const uint triangleStride = 3;
    const uint primitiveOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset + triangleIdx * triangleStride + triangleVertexIdx;

    // Load multiple of 4
    const uint primitiveOffsetToLoad = primitiveOffset & 0xfffffffc;

    const uint meshletPrimitiveData = u_geometryData.Load<uint>(primitiveOffsetToLoad);

    // Load proper byte
    const uint primitiveIdxMask = 0x000000ff;
    const uint primIdx = (meshletPrimitiveData >> ((primitiveOffset - primitiveOffsetToLoad) << 3)) & primitiveIdxMask;
    
    const uint verticesOffset = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;

    const uint vertexIdx = u_geometryData.Load<uint>(verticesOffset + (primIdx * 4));

    const float3 vertexLocation = u_geometryData.Load<float3>(submesh.locationsOffset + vertexIdx * 12);
    const float3 vertexWorldLocation = mul(instanceTransform, float4(vertexLocation, 1.f)).xyz;
    output.clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(vertexWorldLocation, 1.f));

    output.hasTangent = false;
    if(submesh.normalsOffset != IDX_NONE_32)
    {
        const float3 vertexNormal = u_geometryData.Load<float3>(submesh.normalsOffset + vertexIdx * 12);
        output.normal = normalize(mul(instanceTransform, float4(vertexNormal, 0.f)).xyz);
        
        if(submesh.tangentsOffset != IDX_NONE_32)
        {
            const float4 vertexTangent = u_geometryData.Load<float4>(submesh.tangentsOffset + vertexIdx * 16);
            output.tangent = normalize(mul(instanceTransform, float4(vertexTangent.xyz, 0.f)).xyz);
            output.bitangent = normalize(cross(output.normal, output.tangent)) * vertexTangent.w;

            if (!any(isnan(output.bitangent)))
            {
                output.hasTangent = true;
            }
        }
    }
    
    if(submesh.uvsOffset != IDX_NONE_32)
    {
        output.uv = u_geometryData.Load<float2>(submesh.uvsOffset + vertexIdx * 8);
    }

    output.materialDataID = u_visibleBatchElements[batchElementIdx].materialDataID;

    output.worldLocation = vertexWorldLocation;
    output.pixelClipSpace = output.clipSpace;

    return output;
}


struct FO_PS_OUTPUT
{
    float4 luminance              : SV_TARGET0;
    float4 eyeAdaptationLuminance : SV_TARGET1;
    float4 normal                 : SV_TARGET2;
    float4 specularAndRoguhness   : SV_TARGET3;
};


FO_PS_OUTPUT SMForwardOpaque_FS(VS_OUTPUT vertexInput)
{
    FO_PS_OUTPUT output;

    MaterialEvaluationParameters materialEvalParams;
    materialEvalParams.normal = vertexInput.normal;
    materialEvalParams.tangent = vertexInput.tangent;
    materialEvalParams.bitangent = vertexInput.bitangent;
    materialEvalParams.hasTangent = vertexInput.hasTangent;
    materialEvalParams.uv = vertexInput.uv;
    materialEvalParams.worldLocation = vertexInput.worldLocation;
    materialEvalParams.clipSpace = vertexInput.pixelClipSpace;
    
    const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(vertexInput.materialDataID);

#ifdef SPT_MATERIAL_CUSTOM_OPACITY
    const CustomOpacityOutput opacityOutput = EvaluateCustomOpacity(materialEvalParams, materialData);
    if(opacityOutput.shouldDiscard)
    {
        discard;
    }
#endif // SPT_MATERIAL_CUSTOM_OPACITY

    const MaterialEvaluationOutput evaluatedMaterial = EvaluateMaterial(materialEvalParams, materialData);

    const float2 screenUV = vertexInput.pixelClipSpace.xy / vertexInput.pixelClipSpace.w * 0.5f + 0.5f;

    ShadedSurface surface;
    surface.location = vertexInput.worldLocation;
    surface.shadingNormal = evaluatedMaterial.shadingNormal;
    surface.geometryNormal = evaluatedMaterial.geometryNormal;
    surface.roughness = evaluatedMaterial.roughness;
    surface.uv = screenUV;
    surface.linearDepth = ComputeLinearDepth(vertexInput.pixelClipSpace.z / vertexInput.pixelClipSpace.w, u_sceneView);

    ComputeSurfaceColor(evaluatedMaterial.baseColor, evaluatedMaterial.metallic, OUT surface.diffuseColor, OUT surface.specularColor);

    const float3 toView = normalize(u_sceneView.viewLocation - vertexInput.worldLocation);

	ViewLightingAccumulator lightingAccumulator = ViewLightingAccumulator::Create();
    CalcReflectedLuminance(surface, toView, INOUT lightingAccumulator);

    float3 indirectIlluminance = 0.f;

    float ambientOcclusion = 1.f;
   
#ifdef ENABLE_AMBIENT_OCCLUSION

    ambientOcclusion = u_ambientOcclusionTexture.SampleLevel(u_nearestSampler, screenUV, 0.f);

#endif // ENABLE_AMBIENT_OCCLUSION

#ifdef ENABLE_DDGI

    DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(vertexInput.worldLocation, surface.geometryNormal, toView);
    ddgiSampleParams.sampleDirection = surface.shadingNormal;
    indirectIlluminance = DDGISampleLuminanceBlended(ddgiSampleParams) * ambientOcclusion;

#endif // ENABLE_DDGI

	const float3 indirectDiffuse = Diffuse_Lambert(indirectIlluminance);
	lightingAccumulator.Accumulate(LightingContribution::Create(surface.diffuseColor * indirectDiffuse, indirectDiffuse));

	lightingAccumulator.Accumulate(LightingContribution::Create(evaluatedMaterial.emissiveColor));

	const float3 luminance              = lightingAccumulator.GetLuminance();
	const float3 eyeAdaptationLuminance = lightingAccumulator.GetEyeAdaptationLuminance();

	output.luminance              = float4(LuminanceToExposedLuminance(luminance), 1.f);
	output.eyeAdaptationLuminance = float4(LuminanceToExposedLuminance(eyeAdaptationLuminance), 1.f);
    
    output.normal               = float4(surface.shadingNormal * 0.5f + 0.5f, 1.f);
    output.specularAndRoguhness = float4(surface.specularColor, evaluatedMaterial.roughness);

    return output;
}
