#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "RenderStages/ForwardOpaque/ForwardOpaque.hlsli"
#include "Utils/SceneViewUtils.hlsli"

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

[[descriptor_set(DDGIDS, 9)]]

#include "DDGI/DDGITypes.hlsli"

#endif // ENABLE_DDGI

[[shader_struct(MaterialPBRData)]]

#include "Lights/Lighting.hlsli"


struct VS_INPUT
{
    uint index : SV_VertexID;
};


struct VS_OUTPUT
{
    float4  clipSpace           : SV_POSITION;

    float3  normal              : VERTEX_NORMAL;
    uint    materialDataOffset  : MATERIAL_DATA;
    float3  tangent             : VERTEX_TANGENT;
    bool    hasTangent          : VERTEX_HAS_TANGENT;
    float3  bitangent           : VERTEX_BITANGENT;
    float2  uv                  : VERTEX_UV;
    
    float3  worldLocation       : WORLD_LOCATION;
    float4  pixelClipSpace      : CLIP_SPACE;


#if WITH_DEBUGS
    uint    meshletIdx          : MESHLET_IDX;
#endif // WITH_DEBUGS
};


VS_OUTPUT StaticMeshVS(VS_INPUT input)
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

#if WITH_DEBUGS
    if(u_viewRenderingParams.debugFeatureIndex == SPT_DEBUG_FEATURE_MESHLETS)
    {
        output.meshletIdx = meshletIdx;
    }
#endif // WITH_DEBUGS

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

    output.materialDataOffset = u_visibleBatchElements[batchElementIdx].materialDataOffset;

    output.worldLocation = vertexWorldLocation;
    output.pixelClipSpace = output.clipSpace;

    return output;
}


FO_PS_OUTPUT StaticMeshFS(VS_OUTPUT vertexInput)
{
    FO_PS_OUTPUT output;
    
    float4 baseColor = 1.f;
    
    const MaterialPBRData material = u_materialsData.Load<MaterialPBRData>(vertexInput.materialDataOffset);
    if(material.baseColorTextureIdx != IDX_NONE_32)
    {
        baseColor = u_materialsTextures[material.baseColorTextureIdx].Sample(u_materialTexturesSampler, vertexInput.uv);
        baseColor.rgb = pow(baseColor.rgb, 2.2f);
    }

    baseColor.rgb *= material.baseColorFactor;

    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    
    if(material.metallicRoughnessTextureIdx != IDX_NONE_32)
    {
        float2 metallicRoughness = u_materialsTextures[material.metallicRoughnessTextureIdx].Sample(u_materialTexturesSampler, vertexInput.uv).xy;
        metallic *= metallicRoughness.x;
        roughness *= metallicRoughness.y;
    }

    float3 shadingNormal;
    const float3 geometryNormal = normalize(vertexInput.normal);
    if(vertexInput.hasTangent && material.normalsTextureIdx != IDX_NONE_32)
    {
        float3 textureNormal = u_materialsTextures[material.normalsTextureIdx].Sample(u_materialTexturesSampler, vertexInput.uv).xyz;
        textureNormal = textureNormal * 2.f - 1.f;
        textureNormal.z = sqrt(1.f - saturate(Pow2(textureNormal.x) + Pow2(textureNormal.y)));
        const float3x3 TBN = transpose(float3x3(normalize(vertexInput.tangent), normalize(vertexInput.bitangent), geometryNormal));
        shadingNormal = normalize(mul(TBN, textureNormal));
    }
    else
    {
        shadingNormal = normalize(vertexInput.normal);
    }

#if WITH_DEBUGS
    float3 indirectLighting = 0.f;
#endif // WITH_DEBUGS

    if(baseColor.a > 0.9f)
    {
        const float2 screenUV = vertexInput.pixelClipSpace.xy / vertexInput.pixelClipSpace.w * 0.5f + 0.5f;

        ShadedSurface surface;
        surface.location = vertexInput.worldLocation;
        surface.shadingNormal = shadingNormal;
        surface.geometryNormal = geometryNormal;
        surface.roughness = roughness;
        surface.uv = screenUV;
        surface.linearDepth = ComputeLinearDepth(vertexInput.pixelClipSpace.z / vertexInput.pixelClipSpace.w, GetNearPlane(u_sceneView.projectionMatrix));

        ComputeSurfaceColor(baseColor.rgb, metallic, surface.diffuseColor, surface.specularColor);

        const float3 toView = normalize(u_sceneView.viewLocation - vertexInput.worldLocation);

        float3 radiance = CalcReflectedRadiance(surface, u_sceneView.viewLocation);

        float3 indirect = 0.f;

#ifdef ENABLE_DDGI

        indirect = SampleIrradiance(u_ddgiParams, u_probesIrradianceTexture, u_probesDataSampler, u_probesHitDistanceTexture, u_probesDataSampler, vertexInput.worldLocation, shadingNormal, toView);

#endif // ENABLE_DDGI

        radiance += surface.diffuseColor * indirect;

#if WITH_DEBUGS
        indirectLighting = indirect;
#endif // WITH_DEBUGS

        output.radiance = float4(radiance, 1.f);
    }
    else
    {
        output.radiance = 0.f;
    }
    
    output.normal = float4(shadingNormal * 0.5f + 0.5f, 1.f);

#if WITH_DEBUGS
    float3 debug = 1.f;
    if(u_viewRenderingParams.debugFeatureIndex == SPT_DEBUG_FEATURE_MESHLETS)
    {
        const uint meshletHash = HashPCG(vertexInput.meshletIdx);
        debug = float3(float(meshletHash & 255), float((meshletHash >> 8) & 255), float((meshletHash >> 16) & 255)) / 255.0;
    }
    else if(u_viewRenderingParams.debugFeatureIndex == SPT_DEBUG_FEATURE_INDIRECT_LIGHTING)
    {
        debug = indirectLighting;
    }
    output.debug = float4(debug, 1.f);
#endif // WITH_DEBUGS

    return output;
}
