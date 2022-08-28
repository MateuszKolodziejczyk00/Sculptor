#type(compute) //===========================================================
#version 460
#include "SculptorShader.glslh"

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0,  rgba8) uniform image2D u_texture;

void main()
{
	const Int2 currentPixel = Int2(gl_GlobalInvocationID.xy);

	imageStore(u_texture, currentPixel, Float4(1.0));
}
