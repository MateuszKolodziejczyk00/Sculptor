/* Based on https://frostbite-wp-prd.s3.amazonaws.com/wp-content/uploads/2016/03/29204330/GDC_2016_Compute.pdf slide 38 */
uint GetCompactedIndex(uint2 valueBallot, uint laneIdx)
{
    uint2 compactMask;
    compactMask.x = laneIdx >= 32 ? ~0 : ((1U <<  laneIdx) - 1);
    compactMask.y = laneIdx  < 32 ?  0 : ((1U << (laneIdx - 32)) - 1);
    return countbits(valueBallot.x & compactMask.x)
         + countbits(valueBallot.y & compactMask.y);
}


uint2 ReorderWaveToQuads(in uint threadIdx)
{
	const uint offsetX = ((threadIdx & 0x4) >> 1u) + ((threadIdx & 0x10) >> 2u) + (threadIdx & 0x1);

	uint offsetY = ((threadIdx & 0x8) >> 2u) + ((threadIdx & 0x2) >> 1u);
	if(WaveGetLaneCount() == 64u)
	{
		offsetY += ((threadIdx & 0x20) >> 3u);
	}

	return uint2(offsetX, offsetY);
}
