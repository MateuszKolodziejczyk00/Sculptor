#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneTypes.h"
#include "ShaderStructs.h"
#include "MaterialsUnifiedData.h"
#include "RayTracing/RayTracingSceneTypes.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(GPUSceneData)
	SHADER_STRUCT_FIELD(Real32,              time)
	SHADER_STRUCT_FIELD(Real32,              deltaTime)
	SHADER_STRUCT_FIELD(Uint32,              frameIdx)
	SHADER_STRUCT_FIELD(RenderEntitiesArray, renderEntitiesArray)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SceneGeometryData)
	SHADER_STRUCT_FIELD(StaticMeshGeometryBuffers, staticMeshGeometryBuffers)
	SHADER_STRUCT_FIELD(UnifiedGeometryBuffer,     ugb)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GPUMaterialsData)
	SHADER_STRUCT_FIELD(mat::MaterialUnifiedData, data)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(RenderSceneConstants)
	SHADER_STRUCT_FIELD(GPUSceneData,      gpuScene)
	SHADER_STRUCT_FIELD(RTSceneData,       rtScene)
	SHADER_STRUCT_FIELD(SceneGeometryData, geometry)
	SHADER_STRUCT_FIELD(GPUMaterialsData,  materials)
	SHADER_STRUCT_FIELD(ShadowMapsData,    shadows)
END_SHADER_STRUCT();


DS_BEGIN(RenderSceneDS, rg::RGDescriptorSetState<RenderSceneDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingStaticOffset<RenderSceneConstants>), u_renderSceneConstants)
DS_END();

} // spt::rsc
