#ifndef PERLIN_NOISE_HLSLI
#define PERLIN_NOISE_HLSLI

#define PERMUTATION_TABLE_SIZE 256
#define PERMUTATION_TABLE_IDX_MASK 255
#define GRADIENTS_NUM 16
#define GRADIENTS_IDX_MASK 15

namespace perlin_noise
{

    static uint permutationTable[PERMUTATION_TABLE_SIZE] =
    {
        93, 30, 236, 192, 94, 95, 141, 19, 234, 27, 13, 218, 131, 85, 246, 155,
        37, 24, 156, 115, 132, 245, 201, 250, 44, 127, 181, 21, 98, 87, 158, 160,
        38, 198, 114, 67, 0, 190, 78, 68, 133, 175, 7, 173, 152, 73, 189, 120,
        97, 109, 71, 55, 139, 200, 166, 157, 91, 86, 196, 72, 40, 36, 253, 20,
        119, 2, 212, 248, 249, 178, 31, 232, 188, 174, 217, 10, 12, 118, 23, 3,
        75, 140, 203, 103, 187, 186, 110, 134, 209, 54, 14, 136, 243, 169, 151, 208,
        125, 41, 130, 56, 121, 145, 129, 108, 51, 240, 153, 80, 233, 99, 69, 202,
        58, 5, 74, 96, 45, 25, 224, 229, 105, 39, 199, 63, 244, 48, 221, 142,
        146, 90, 216, 8, 176, 213, 57, 164, 18, 60, 70, 49, 116, 159, 251, 35,
        255, 104, 254, 171, 77, 144, 180, 183, 197, 219, 195, 226, 11, 247, 46, 252,
        106, 107, 211, 82, 191, 6, 242, 228, 65, 128, 81, 163, 210, 79, 239, 205,
        66, 62, 161, 206, 147, 26, 215, 83, 230, 32, 124, 138, 84, 238, 137, 123,
        143, 193, 148, 214, 28, 241, 43, 227, 59, 61, 231, 17, 177, 101, 22, 15,
        34, 204, 122, 29, 220, 165, 149, 167, 179, 47, 154, 1, 42, 4, 235, 207,
        222, 223, 9, 92, 185, 182, 50, 194, 225, 162, 102, 16, 112, 100, 89, 64,
        237, 150, 53, 52, 168, 126, 76, 117, 172, 113, 33, 170, 184, 135, 111, 88
    };


    static float3 gradients[GRADIENTS_NUM] =
    {
        float3(1.f, 1.f, 0.f),
        float3(-1.f, 1.f, 0.f),
        float3(1.f, -1.f, 0.f),
        float3(-1.f, -1.f, 0.f),
        float3(1.f, 0.f, 1.f),
        float3(-1.f, 0.f, 1.f),
        float3(1.f, 0.f, -1.f),
        float3(-1.f, 0.f, -1.f),
        float3(0.f, 1.f, 1.f),
        float3(0.f, -1.f, 1.f),
        float3(0.f, 1.f, -1.f),
        float3(0.f, -1.f, -1.f),
        float3(1.f, 1.f, 0.f),
        float3(-1.f, 1.f, 0.f),
        float3(0.f, -1.f, 1.f),
        float3(0.f, -1.f, -1.f)
    };

    
    float3 GetGradient(int3 idx)
    {
        uint i = idx.x & PERMUTATION_TABLE_IDX_MASK;
        i = (permutationTable[i] + idx.y) & PERMUTATION_TABLE_IDX_MASK;
        i = (permutationTable[i] + idx.z) & PERMUTATION_TABLE_IDX_MASK;
        i = permutationTable[i] & GRADIENTS_IDX_MASK;
        return gradients[i];
    }

    
    float3 Quintic(in float3 t) 
    { 
        return t * t * t * (t * (t * 6.f - 15.f) + 10.f); 
    } 


    float Evaluate(in float3 coords)
    {
        int3 floorCoords = coords;
        float3 fractCoords = coords - floorCoords;
        if(fractCoords.x < 0.f)
        {
            fractCoords.x += 1.f;
            floorCoords.x -= 1;
        }
        if (fractCoords.y < 0.f)
        {
            fractCoords.y += 1.f;
            floorCoords.y -= 1;
        }
        if (fractCoords.z < 0.f)
        {
            fractCoords.z += 1.f;
            floorCoords.z -= 1;
        }
    
        const float3 g000 = GetGradient(floorCoords + int3(0, 0, 0));
        const float3 g001 = GetGradient(floorCoords + int3(0, 0, 1));
        const float3 g010 = GetGradient(floorCoords + int3(0, 1, 0));
        const float3 g011 = GetGradient(floorCoords + int3(0, 1, 1));
        const float3 g100 = GetGradient(floorCoords + int3(1, 0, 0));
        const float3 g101 = GetGradient(floorCoords + int3(1, 0, 1));
        const float3 g110 = GetGradient(floorCoords + int3(1, 1, 0));
        const float3 g111 = GetGradient(floorCoords + int3(1, 1, 1));

        const float dot000 = dot(g000, fractCoords - float3(0.f, 0.f, 0.f));
        const float dot001 = dot(g001, fractCoords - float3(0.f, 0.f, 1.f));
        const float dot010 = dot(g010, fractCoords - float3(0.f, 1.f, 0.f));
        const float dot011 = dot(g011, fractCoords - float3(0.f, 1.f, 1.f));
        const float dot100 = dot(g100, fractCoords - float3(1.f, 0.f, 0.f));
        const float dot101 = dot(g101, fractCoords - float3(1.f, 0.f, 1.f));
        const float dot110 = dot(g110, fractCoords - float3(1.f, 1.f, 0.f));
        const float dot111 = dot(g111, fractCoords - float3(1.f, 1.f, 1.f));

        const float3 weights = Quintic(fractCoords);

        const float dot00 = lerp(dot000, dot100, weights.x);
        const float dot01 = lerp(dot001, dot101, weights.x);
        const float dot10 = lerp(dot010, dot110, weights.x);
        const float dot11 = lerp(dot011, dot111, weights.x);
    
        const float dot0 = lerp(dot00, dot10, weights.y);
        const float dot1 = lerp(dot01, dot11, weights.y);
    
        return lerp(dot0, dot1, weights.z);
    }

}

#endif // PERLIN_NOISE_HLSLI