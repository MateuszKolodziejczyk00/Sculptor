#ifndef GPU_MATERIALS_HLSLI
#define GPU_MATERIALS_HLSLI

#ifdef DS_RenderSceneDS

[[shader_struct(GPUMaterialsData)]]


struct GPUMaterialsInterface : GPUMaterialsData
{

};


GPUMaterialsInterface GPUMaterials()
{
	return GPUMaterialsInterface(u_renderSceneConstants.materials);
}
#endif // DS_RenderSceneDS

#endif // GPU_MATERIALS_HLSLI
