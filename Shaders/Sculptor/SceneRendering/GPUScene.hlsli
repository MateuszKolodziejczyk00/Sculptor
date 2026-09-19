#ifndef GPU_SCENE_HLSLI
#define GPU_SCENE_HLSLI
#ifdef PARAM_RenderSceneConstants

#include "RayTracing/RTScene.hlsli"
#include "SceneRendering/UGB.hlsli"

[[shader_struct(GPUSceneData)]]


extension GPUSceneData
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


typealias GPUSceneInterface = GPUSceneData;

GPUSceneInterface GPUScene()
{
	return SCENE->gpuScene;
}

SPT_NAMED_DESCRIPTOR(RenderEntitiesArray, SCENE->gpuScene.renderEntitiesArray.GetIndex())
#endif // PARAM_RenderSceneConstants

#endif // GPU_SCENE_HLSLI
