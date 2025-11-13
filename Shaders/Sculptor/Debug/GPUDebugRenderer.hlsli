#ifndef GPU_DEBUG_RENDERER_HLSLI
#define GPU_DEBUG_RENDERER_HLSLI


#if SPT_META_PARAM_DEBUG_FEATURES

[[shader_struct(DebugLineDefinition)]]

struct GPUDebugRenderer
{
	static void DrawLine(DebugLineDefinition lineDef)
	{
		const uint lineIdx = u_debugCommandsBufferParams.dynamicDebugRendererData.rwLinesNum.AtomicAdd(0u, 1u);
		u_debugCommandsBufferParams.dynamicDebugRendererData.rwLines.Store(lineIdx, lineDef);
	}

	static void DrawPersistentLine(DebugLineDefinition lineDef)
	{
		const uint lineIdx = u_debugCommandsBufferParams.persistentDebugRendererData.rwLinesNum.AtomicAdd(0u, 1u);
		u_debugCommandsBufferParams.persistentDebugRendererData.rwLines.Store(lineIdx, lineDef);
	}

	static void DrawMarker(DebugMarkerDefinition markerDef)
	{
		const uint markerIdx = u_debugCommandsBufferParams.dynamicDebugRendererData.rwMarkersNum.AtomicAdd(0u, 1u);
		u_debugCommandsBufferParams.dynamicDebugRendererData.rwMarkers.Store(markerIdx, markerDef);
	}

	static void DrawPersistentMarker(DebugMarkerDefinition markerDef)
	{
		const uint markerIdx = u_debugCommandsBufferParams.persistentDebugRendererData.rwMarkersNum.AtomicAdd(0u, 1u);
		u_debugCommandsBufferParams.persistentDebugRendererData.rwMarkers.Store(markerIdx, markerDef);
	}

	static void DrawSphere(DebugSphereDefinition sphereDef)
	{
		const uint sphereIdx = u_debugCommandsBufferParams.dynamicDebugRendererData.rwSpheresNum.AtomicAdd(0u, 1u);
		u_debugCommandsBufferParams.dynamicDebugRendererData.rwSpheres.Store(sphereIdx, sphereDef);
	}

	static void DrawPersistentSphere(DebugSphereDefinition sphereDef)
	{
		const uint sphereIdx = u_debugCommandsBufferParams.persistentDebugRendererData.rwSpheresNum.AtomicAdd(0u, 1u);
		u_debugCommandsBufferParams.persistentDebugRendererData.rwSpheres.Store(sphereIdx, sphereDef);
	}
};

#endif // SPT_META_PARAM_DEBUG_FEATURES

#endif // GPU_DEBUG_RENDERER_HLSLI
