#ifndef GPU_MATERIALS_HLSLI
#define GPU_MATERIALS_HLSLI

#ifdef PARAM_RenderSceneConstants

[[shader_struct(GPUMaterialsData)]]


extension GPUMaterialsData
{

};

typealias GPUMaterialsInterface = GPUMaterialsData;

GPUMaterialsInterface GPUMaterials()
{
	return SCENE->materials;
}
#endif // PARAM_RenderSceneConstants

#endif // GPU_MATERIALS_HLSLI
