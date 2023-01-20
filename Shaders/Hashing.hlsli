
/** Source: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/ */
uint HashPCG(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}