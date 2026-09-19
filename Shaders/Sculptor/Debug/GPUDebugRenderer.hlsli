#ifndef GPU_DEBUG_RENDERER_HLSLI
#define GPU_DEBUG_RENDERER_HLSLI


#if SPT_META_PARAM_DEBUG_FEATURES

[[shader_struct(DebugLineDefinition)]]

struct GPUDebugRenderer
{
	static void DrawLine(DebugLineDefinition lineDef)
	{
		const uint lineIdx = PARAM_ShaderDebugCommandBufferParams->dynamicDebugRendererData.rwLinesNum.AtomicAdd(0u, 1u);
		PARAM_ShaderDebugCommandBufferParams->dynamicDebugRendererData.rwLines.Store(lineIdx, lineDef);
	}

	static void DrawPersistentLine(DebugLineDefinition lineDef)
	{
		const uint lineIdx = PARAM_ShaderDebugCommandBufferParams->persistentDebugRendererData.rwLinesNum.AtomicAdd(0u, 1u);
		PARAM_ShaderDebugCommandBufferParams->persistentDebugRendererData.rwLines.Store(lineIdx, lineDef);
	}

	static void DrawMarker(DebugMarkerDefinition markerDef)
	{
		const uint markerIdx = PARAM_ShaderDebugCommandBufferParams->dynamicDebugRendererData.rwMarkersNum.AtomicAdd(0u, 1u);
		PARAM_ShaderDebugCommandBufferParams->dynamicDebugRendererData.rwMarkers.Store(markerIdx, markerDef);
	}

	static void DrawPersistentMarker(DebugMarkerDefinition markerDef)
	{
		const uint markerIdx = PARAM_ShaderDebugCommandBufferParams->persistentDebugRendererData.rwMarkersNum.AtomicAdd(0u, 1u);
		PARAM_ShaderDebugCommandBufferParams->persistentDebugRendererData.rwMarkers.Store(markerIdx, markerDef);
	}

	static void DrawSphere(DebugSphereDefinition sphereDef)
	{
		const uint sphereIdx = PARAM_ShaderDebugCommandBufferParams->dynamicDebugRendererData.rwSpheresNum.AtomicAdd(0u, 1u);
		PARAM_ShaderDebugCommandBufferParams->dynamicDebugRendererData.rwSpheres.Store(sphereIdx, sphereDef);
	}

	static void DrawPersistentSphere(DebugSphereDefinition sphereDef)
	{
		const uint sphereIdx = PARAM_ShaderDebugCommandBufferParams->persistentDebugRendererData.rwSpheresNum.AtomicAdd(0u, 1u);
		PARAM_ShaderDebugCommandBufferParams->persistentDebugRendererData.rwSpheres.Store(sphereIdx, sphereDef);
	}
};

#endif // SPT_META_PARAM_DEBUG_FEATURES

#endif // GPU_DEBUG_RENDERER_HLSLI
