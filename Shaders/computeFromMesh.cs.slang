#include "mesh.slang"

#define SDF_TYPE_SDF0 10

#ifndef SDF_TYPE
#define SDF_TYPE SDF_TYPE_SDF0
#endif

cbuffer CScb
{
    uint3 currentInputOffset;  // .x: input offset, .y: input size, .z: [unused]
    uint3 currentOutputOffset; // .xyz: output offset
    uint3 maxSize;             // output resolution
    float3 oneOverMaxSize;     // = 1/maxSize

    float3 BBcorner; // output bounding box
    float3 BBsize;   // output bounding box
};

RWTexture3D<float4> outSDF;
RWTexture3D<float4> tex2;

float3 texCoord(uint3 texelInd)
{
    return ((float3) texelInd + 0.5) * oneOverMaxSize;
}

[numthreads(8, 8, 8)]
void initMeshCalc_main(uint3 threadId : SV_DispatchThreadID)
{
    if (any(threadId >= maxSize))
        return;
    
    outSDF[threadId] = encodeMeshCalcData(initMeshCalcData());
}

[numthreads(8, 8, 8)]
void calcMesh_main(uint3 threadId : SV_DispatchThreadID)
{
    const uint3 outputIndex = currentOutputOffset + threadId.xyz;
    
    if ( any(outputIndex >= maxSize) )
        return;
    
    const float3 tex = texCoord(outputIndex); // texture coords
    const float3 posW = BBcorner + tex * BBsize;

    MeshCalcData data = decodeMeshCalcData(outSDF[outputIndex]);
    MeshCalcData newData = processMeshChunk(currentInputOffset, posW, data, BBcorner + 0.5 * BBsize, BBsize * oneOverMaxSize);
    outSDF[outputIndex] = encodeMeshCalcData(newData);
}

[numthreads(8, 8, 8)]
void finishMeshCalc_main(uint3 threadId : SV_DispatchThreadID)
{
    if (any(threadId >= maxSize))
        return;

    MeshCalcData data = decodeMeshCalcData(tex2[threadId]);
    uint3 ii = data.intersections % 2;
    float sgn = (ii.x + ii.y + ii.z <= 1) ? 1 : -1;
    float sdf = sgn * sqrt(data.SDFsq);

#if SDF_TYPE == SDF_TYPE_SDF0
    outSDF[threadId] = float4(sdf, 0, 0, 0);
#else
#error "Set SDF_TYPE to a supported type"
#endif
    tex2[threadId] = float4(sdf, 0, 0, 0);
}
