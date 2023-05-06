#ifndef SDF_SLANG_INCLUDED
#define SDF_SLANG_INCLUDED

#ifndef PROCEDURAL_FUNCTION_FILE
#include "SDFScenes/sphere.slang"
#else
// computed include -- my syntax highlighter doesn't like it
#include PROCEDURAL_FUNCTION_FILE
#endif

#ifndef SDF_SOURCE
#define SDF_SOURCE 0
// SDF_SOURCE == 0 : procedural sdf
// SDF_SOURCE == 1 : DSDF (texture)
// SDF_SOURCE == 2 : mesh
#endif

#include "sdf_model.slang"


SamplerState sdfSampler;


float getTextureSdfSample(float3 x)
{
    return modelTex.SampleLevel(sdfSampler, x, 0).r;
}
float getProceduralSdfSample(float3 x)
{
    float3 wpos = outerBoxSize * x + outerBoxCorner;
    return funDist(wpos);
}


// x: texture coordinates
float getSdfSample(float3 x)
{
#if SDF_SOURCE == 0
    return getProceduralSdfSample(x);
#elif SDF_SOURCE == 1
    return getTextureSdfSample(x);
#elif SDF_SOURCE == 2
    return getMeshSdfSample(x);
#endif
}

// p: local model coordinates (origin = outerBoxCorner)
float sdfInside(float3 p)
{
    float3 texCoord = p * oneOverOuterBoxSize;
    return getSdfSample(texCoord);
}

// p: world coordinates
float sdf(float3 p)
{
    float3 q = p - outerBoxCorner;
    return sdfInside(q);
}



#endif
