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
};

#endif // SPT_META_PARAM_DEBUG_FEATURES

#endif // GPU_DEBUG_RENDERER_HLSLI
