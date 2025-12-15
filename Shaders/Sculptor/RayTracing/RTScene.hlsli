#ifndef RTSCENE_HLSLI
#define RTSCENE_HLSLI

#include "RayTracing/RayTracingPayload.hlsli"


#define MATERIAL_RT_DOUBLE_SIDED 1

[[shader_struct(RTInstanceData)]]


[[override]]
struct RTInstanceInterface : RTInstanceData
{
	bool IsDoubleSided() 
	{ 
		return (metarialRTFlags & MATERIAL_RT_DOUBLE_SIDED) != 0; 
	}
};


[[shader_struct(RTSceneData)]]


struct RTSceneInterface : RTSceneData
{
	bool VisibilityTest(in RayDesc rayDesc);

#ifdef RT_MATERIAL_TRACING
	RayPayloadData TraceMaterialRay(in RayDesc rayDesc);
#endif // RTMaterialsMissShaderTag

	GPUPtr<RTInstanceInterface> GetInstancePtr(uint instanceIdx)
	{
		return rtInstances.PtrAt(instanceIdx);
	}
};


bool RTSceneInterface::VisibilityTest(in RayDesc rayDesc)
{
	RayPayloadData payload = RayPayloadData::Init();

	const uint instanceMask = RT_INSTANCE_FLAG_OPAQUE;

	TraceRay(tlas.GetResource(),
			 RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
			 instanceMask,
			 0,
			 1,
			 0,
			 rayDesc,
			 payload);

	return payload.visibility.isMiss;
}


#ifdef RT_MATERIAL_TRACING
RayPayloadData RTSceneInterface::TraceMaterialRay(in RayDesc rayDesc)
{
	RayPayloadData payload = RayPayloadData::Init();

	const uint instanceMask = RT_INSTANCE_FLAG_OPAQUE;

	TraceRay(tlas.GetResource(),
			 0,
			 instanceMask,
			 0,
			 1,
			 0,
			 rayDesc,
			 payload);

	return payload;
}
#endif // RT_MATERIAL_TRACING


#ifdef DS_RenderSceneDS
RTSceneInterface RTScene()
{
	return RTSceneInterface(u_renderSceneConstants.rtScene);
}
#endif // DS_RenderSceneDS

#endif // RTSCENE_HLSLI