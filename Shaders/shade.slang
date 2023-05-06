#ifndef SHADE_SLANG_INCLUDED
#define SHADE_SLANG_INCLUDED

#include "box_ray_intersecion.slang"
#include "types.slang"
#include "trace.slang"
#include "sdf.slang"

#ifndef MIRROR_BACK_NORMAL
#define MIRROR_BACK_NORMAL 0
#endif

cbuffer SHADEcb
{
    float3 shadeNormalEps;
    float shadowNormalEps;
    float3 lightDir;
    float3 colorAmbient;
    float3 colorDiffuse;
};



float3 getNormalCentralDiff(float3 p)
{
    float4 e0 = float4(shadeNormalEps, 0);
    float3 plus = float3(sdf(p + e0.xww), sdf(p + e0.wyw), sdf(p + e0.wwz));
    float3 mins = float3(sdf(p - e0.xww), sdf(p - e0.wyw), sdf(p - e0.wwz));
    float3 diff = plus - mins;
    return normalize(diff / shadeNormalEps);
}

float3 getNormalForwardDiff(float3 p)
{
    float4 e0 = float4(shadeNormalEps, 0);
    float3 plus = float3(sdf(p + e0.xww), sdf(p + e0.wyw), sdf(p + e0.wwz));
    float3 diff = plus - sdf(p);
    return normalize(diff / shadeNormalEps);
}

float3 getNormalScreenspace(float3 p)
{
    float3 pdx = ddx_fine(p);
    float3 pdy = ddy_fine(p);
    return -normalize(cross(pdx, pdy));
}


float3 getNormal(float3 p)
{
#ifdef SCREENSPACE_NORMAL
    return getNormalScreenspace(p);
#endif
#ifdef FORWARD_DIFF_NORMAL
    return getNormalForwardDiff(p);
#endif
    return getNormalCentralDiff(p);
}

// mirror the normal if it points away from the camera
float3 mirrorNormal(float3 n, float3 camDir)
{
    return dot(camDir, n) > 0 ? reflect(n, camDir) : n;
}

float3 shade_normal(Ray ray, float3 pos, float3 norm, ITracer tracer, SphereTraceDesc trDesc)
{
#if MIRROR_BACK_NORMAL
    norm = mirrorNormal(norm, ray.dir);
#endif
    const float3 toLight = -lightDir;
    float diffuseCoeff = saturate(dot(norm, toLight));
#if CALC_HARD_SHADOW
    if (diffuseCoeff != 0)
    {
        Ray shadowRay;
        shadowRay.orig = pos + shadowNormalEps*norm;
        shadowRay.dir = toLight;
        shadowRay.tMin = 2*trDesc.epsilon;
        const Box box = { innerBoxCorner + 0.5 * innerBoxSize, 0.5 * innerBoxSize };
        if (intersectBox(box, shadowRay, false, shadowRay.tMax))
        {
            trDesc.shadowRay = true;
            TraceResult res = tracer.trace(shadowRay, trDesc);
            diffuseCoeff = res.T < shadowRay.tMax ? 0 : diffuseCoeff;
        }
    }
#endif
    return colorAmbient + colorDiffuse * diffuseCoeff;
}

float3 shade(Ray ray, float t, ITracer tracer, SphereTraceDesc trDesc)
{
    const float3 pos = ray.orig + ray.dir * t;
    float3 norm = getNormal(pos);
    return shade_normal(ray, pos, norm, tracer, trDesc);
}

#endif
