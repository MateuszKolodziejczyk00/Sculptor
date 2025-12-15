#ifndef GPU_MATERIALS_HLSLI
#define GPU_MATERIALS_HLSLI

[[shader_struct(GPUMaterialsData)]]


struct GPUMaterialsInterface : GPUMaterialsData
{

};


#ifdef DS_RenderSceneDS
GPUMaterialsInterface GPUMaterials()
{
	return GPUMaterialsInterface(u_renderSceneConstants.materials);
}
#endif // DS_RenderSceneDS

#endif // GPU_MATERIALS_HLSLI
