#ifndef RTSCENE_HLSLI
#define RTSCENE_HLSLI

#include "RayTracing/RayTracingPayload.hlsli"


#define MATERIAL_RT_DOUBLE_SIDED 1

[[shader_struct(RTInstanceData)]]


extension RTInstanceData
{
	bool IsDoubleSided() 
	{ 
		return (metarialRTFlags & MATERIAL_RT_DOUBLE_SIDED) != 0; 
	}
};


typealias RTInstanceInterface = RTInstanceData;


[[shader_struct(RTSceneData)]]


extension RTSceneData
{
	bool VisibilityTest(in RayDesc rayDesc)
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
	RayPayloadData TraceMaterialRay(in RayDesc rayDesc)
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
#endif // RTMaterialsMissShaderTag

	GPUPtr<RTInstanceInterface> GetInstancePtr(uint instanceIdx)
	{
		return rtInstances.PtrAt(instanceIdx);
	}
};

typealias RTSceneInterface = RTSceneData;

#ifdef PARAM_RenderSceneConstants
RTSceneInterface RTScene()
{
	return SCENE->rtScene;
}
#endif // PARAM_RenderSceneConstants

#endif // RTSCENE_HLSLI
