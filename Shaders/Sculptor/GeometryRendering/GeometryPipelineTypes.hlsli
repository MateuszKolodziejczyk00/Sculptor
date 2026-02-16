#ifndef GEOMETRY_PIPELINE_TYPES_HLSLI
#define GEOMETRY_PIPELINE_TYPES_HLSLI

struct MeshPayload
{
#if MESHLETS_SHARE_ENTITY
	uint batchElementIdx;
	uint firstLocalMeshletIdx;
	uint validMeshletsMask;
#else
	uint localMeshletIdx[TS_GROUP_SIZE];
	uint batchElementIdx[TS_GROUP_SIZE];

	uint numCommands;
#endif // MESHLETS_SHARE_ENTITY

#ifdef GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE
	GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE  customData;
#endif // GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE
};


struct PerVertexData
{
	float4 locationCS : SV_Position;
#if GEOMETRY_PIPELINE_USE_MATERIAL
	float2 uv : UV;
#endif // GEOMETRY_PIPELINE_USE_MATERIAL
};


struct PerPrimitiveData
{
#ifdef GEOMETRY_PIPELINE_CUSTOM_PER_PRIMITIVE_DATA
	GEOMETRY_PIPELINE_CUSTOM_PER_PRIMITIVE_DATA
#endif // GEOMETRY_PIPELINE_CUSTOM_PER_PRIMITIVE_DATA

#if GEOMETRY_PIPELINE_USE_MATERIAL
	uint materialDataID : MATERIAL_DATA_ID;
#endif // GEOMETRY_PIPELINE_USE_MATERIAL

	// Fix for MeshShadingEXT capability not added to fragment shader
#if SPT_MESH_SHADER
	bool culled               : SV_CullPrimitive;
#endif // SPT_MESH_SHADER
};


struct MeshletDispatchContext
{
	uint localID;
	GeometryBatchElement batchElement;
	SubmeshGPUData submesh;
	RenderEntityGPUData entityData;
	MeshletGPUData meshlet;
	uint localMeshletIdx; // idx of the meshlet within the submesh
	bool isVisible;
	uint compactedLocalVisibleIdx;
};


// Expected signature:
// void DispatchMeshlet(in MeshletDispatchContext context, out GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE customMeshletsData)


struct TriangleDispatchContext
{
	uint groupID;
	uint localID;
	RenderEntityGPUData entityData;
	SubmeshGPUData submesh;
	MeshletGPUData meshlet;
	uint meshletTriangleIdx;
};


// Expected signature:
// void DispatchTriangle(in TriangleDispatchContext context, in GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE customMeshletsData, inout PerPrimitiveData primData)


struct FragmentDispatchContext
{
	PerVertexData    vertData;
	PerPrimitiveData primData;
#if GEOMETRY_PIPELINE_USE_MATERIAL
	SPT_MATERIAL_DATA_TYPE materialData;
#endif // GEOMETRY_PIPELINE_USE_MATERIAL
};


// FRAGMENT_SHADER_OUTPUT_TYPE DispatchFragment(in FragmentDispatchContext context)

#endif // GEOMETRY_PIPELINE_TYPES_HLSLI
