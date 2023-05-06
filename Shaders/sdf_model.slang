#ifndef SDF_MODEL_SLANG_INCLUDED
#define SDF_MODEL_SLANG_INCLUDED

cbuffer MODELcb
{
    float3 innerBoxCorner;
    float3 innerBoxSize;
    float3 outerBoxCorner;
    float3 outerBoxSize;
    float3 oneOverOuterBoxSize;
    uint3 resolution; // SDF (input) resolution
    float3 resolution_r; // reciprocal of SDF (input) resolution
};
Texture3D<float4> modelTex;

#endif
