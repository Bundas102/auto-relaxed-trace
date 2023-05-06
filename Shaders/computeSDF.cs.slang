#include "sdf.slang"


cbuffer CScb : register(b0)
{
    uint3 currentInputOffset;  // current input voxel offset (with rez already resolved)
    uint3 currentOutputOffset; // current output voxel offset (with rez already resolved)
    uint3 maxSize; // output (SDF) resolution
    float3 oneOverMaxSize;

    float3 BBcorner;
    float3 BBsize;
};

RWTexture3D<float4> outSDF;
RWTexture3D<float4> outAuxData;

float3 texCoord(uint3 texelInd)
{
    return ((float3) texelInd + 0.5) * oneOverMaxSize;
}

[numthreads(8, 8, 8)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    uint3 inputIndex = currentInputOffset + threadId.xyz;
    uint3 outputIndex = currentOutputOffset + threadId.xyz;
    
    if ( any(outputIndex >= maxSize) )
        return;
    
    const float3 tex = texCoord(outputIndex); // texture coords
    const float3 posW = BBcorner + tex * BBsize;

    const float sdfVal = sdf(posW);
    
    outSDF[outputIndex] = float4(sdfVal, 0, 0, 0);
    outAuxData[outputIndex] = float4(sdfVal, sdfVal, 0, 0);
}
