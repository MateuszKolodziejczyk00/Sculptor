#ifndef MORTON_CODE_HLSLI
#define MORTON_CODE_HLSLI

// Source: https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/

// "Insert" a 0 bit after each of the 16 low bits of x
uint Part1By1(in uint x)
{
	x &= 0x0000ffff;                  // x = ---- ---- ---- ---- fedc ba98 7654 3210
	x = (x ^ (x <<  8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
	x = (x ^ (x <<  4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
	x = (x ^ (x <<  2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
	x = (x ^ (x <<  1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
	return x;
}

// "Insert" two 0 bits after each of the 10 low bits of x
uint Part1By2(uint x)
{
	x &= 0x000003ff;                  // x = ---- ---- ---- ---- ---- --98 7654 3210
	x = (x ^ (x << 16)) & 0xff0000ff; // x = ---- --98 ---- ---- ---- ---- 7654 3210
	x = (x ^ (x <<  8)) & 0x0300f00f; // x = ---- --98 ---- ---- 7654 ---- ---- 3210
	x = (x ^ (x <<  4)) & 0x030c30c3; // x = ---- --98 ---- 76-- --54 ---- 32-- --10
	x = (x ^ (x <<  2)) & 0x09249249; // x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
	return x;
}

uint EncodeMorton2(in uint2 coords)
{
	return (Part1By1(coords.y) << 1) + Part1By1(coords.x);
}

uint EncodeMorton3(uint x, uint y, uint z)
{
	return (Part1By2(z) << 2) + (Part1By2(y) << 1) + Part1By2(x);
}

#endif // MORTON_CODE_HLSLI
