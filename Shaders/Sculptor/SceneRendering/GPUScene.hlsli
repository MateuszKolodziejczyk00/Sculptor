#ifndef GPU_SCENE_HLSLI
#define GPU_SCENE_HLSLI
#ifdef DS_RenderSceneDS

#include "RayTracing/RTScene.hlsli"
#include "SceneRendering/UGB.hlsli"

[[shader_struct(GPUSceneData)]]


struct GPUSceneInterface : GPUSceneData
{
	GPUPtr<RenderEntityGPUData> GetInstancePtr(uint instanceIdx)
	{
		return renderEntitiesArray.GetElemPtr(instanceIdx);
	}

	RenderEntityGPUData GetInstanceData(in uint instanceIdx)
	{
		return GetInstancePtr(instanceIdx).Load();
	}
};

GPUSceneInterface GPUScene()
{
	return GPUSceneInterface(u_renderSceneConstants.gpuScene);
}
#endif // DS_RenderSceneDS

#endif // GPU_SCENE_HLSLI
