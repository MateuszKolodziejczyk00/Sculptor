#ifndef LIGHTS_GRID_HLSLI
#define LIGHTS_GRID_HLSLI

[[shader_struct(LightsGridCascade)]]
[[shader_struct(LightsGridData)]]


// Must match lights_grid_constants in SceneLightsTypes.h
#define LIGHTS_GRID_CASCADES_NUM    5
#define LIGHTS_GRID_NODES_CAPACITY  0xFFFFu
#define LIGHTS_GRID_LIST_END        0xFFFFu
static const uint3 LightsGridCascadeRes = uint3(16u, 16u, 8u);


uint GetLightsGridCellIdx(in uint cascadeIdx, in uint3 coords)
{
	return ((cascadeIdx * LightsGridCascadeRes.z + coords.z) * LightsGridCascadeRes.y + coords.y) * LightsGridCascadeRes.x + coords.x;
}


extension LightsGridCascade
{
	void GetCellBounds(in uint3 coords, out float3 boundsMin, out float3 boundsMax)
	{
		boundsMin = origin + float3(coords) * cellSize;
		boundsMax = boundsMin + cellSize;
	}

	bool GetCellCoords(in float3 location, out uint3 coords)
	{
		const int3 cellCoords = int3(floor((location - origin) / cellSize));
		coords = uint3(cellCoords);
		return all(cellCoords >= 0) && all(cellCoords < int3(LightsGridCascadeRes));
	}
};


struct LightsGridCellLights
{
	TypedBuffer<uint16_t> nodes;
	uint                  nodeIdx;

	[mutating]
	bool Next(out uint lightIdx)
	{
		lightIdx = nodes.Load(nodeIdx++);
		return lightIdx != LIGHTS_GRID_LIST_END;
	}
};


extension LightsGridData
{
	// Returns empty list for locations outside of all cascades
	LightsGridCellLights GetCellLights(in float3 location)
	{
		LightsGridCellLights cellLights;
		cellLights.nodes   = nodes;
		cellLights.nodeIdx = 0u;

		[unroll]
		for (uint cascadeIdx = 0u; cascadeIdx < LIGHTS_GRID_CASCADES_NUM; ++cascadeIdx)
		{
			uint3 coords;
			if (cascades[cascadeIdx].GetCellCoords(location, OUT coords))
			{
				cellLights.nodeIdx = cells.Load(GetLightsGridCellIdx(cascadeIdx, coords));
				break;
			}
		}

		return cellLights;
	}
};


#ifdef PARAM_RenderSceneConstants

LightsGridData LightsGrid()
{
	return SCENE->lights.lightsGrid;
}

#endif // PARAM_RenderSceneConstants

#endif // LIGHTS_GRID_HLSLI
