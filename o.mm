int LinearIndex;
NeighborGrid3D.IndexToLinear(IndexX, IndexY, IndexZ, LinearIndex);

int NCount; //Ilosc particli w poblizu processowanego pixela
NeighborGrid3D.GetParticleNeighborCount(LinearIndex, NCount);

float minDistance = 0.1;
float maxDistance = 10000;

float totalDistance = 0.0;

for (int steps = 0; steps < 10; steps++)
{
    float3 CurrPos = CamPos + ViewDir * totalDistance;

    float minDistToParticle = 1000000.0;

    for (int i = 0; i < NCount; i++)
    {
        int NeighborLinearIndex = 0;
        NeighborGrid3D.NeighborGridIndexToLinear(IndexX, IndexY, 0, i, NeighborLinearIndex);
    
        int NeighborIndex = 0;
        NeighborGrid3D.GetParticleNeighbor(NeighborLinearIndex, NeighborIndex);
    
        float3 ParticlePos;
        bool bNValid = false;
        PAR.GetVectorByIndex<Attribute="Position">(NeighborIndex, bNValid, ParticlePos);

        if (bNValid) 
        {
            float dist = length(CurrPos - ParticlePos) - radius;
    
            float a = minDistToParticle;
            float b = dist;
            float k = 0.02f;
            float h = 1.0 - min( abs(a-b)/(6.0 * k), 1.0 );
            float w = h * h * h;
            float s = w * k; 
            minDistToParticle = a < b ? a - s : b - s;
        }
    }

    totalDistance += minDistToParticle;

    if (minDistToParticle < minDistance || totalDistance >= maxDistance)
    {
        break;
    }
}

OutDepth = totalDistance;