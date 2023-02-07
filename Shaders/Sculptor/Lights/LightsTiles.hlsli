uint2 GetLightsTile(float2 uv, float2 tileSize)
{
    return uint2(uv / tileSize);
}


uint GetLightsTileDataOffset(uint2 tile, uint2 tilesNum, uint lights32PerTile)
{
    return (tile.y * tilesNum.x + tile.x) * lights32PerTile;
}


uint GetLightsTileDataOffsetForLight(uint2 tile, uint2 tilesNum, uint lights32PerTile, uint lightIdx)
{
    return GetLightsTileDataOffset(tile, tilesNum, lights32PerTile) + (lightIdx / 32);
}
