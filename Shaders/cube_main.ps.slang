#include "types.slang"
#include "box_ray_intersecion.slang"
#include "sdf.slang"
#include "trace.slang"
#include "shade.slang"


cbuffer PScb
{
    float3 camPos;
    float4x4 viewProj;
    
    uint maxStep;
    float traceEpsilon;
    float stepRelaxation;
};

struct PsIn
{
    float3 pos : POSITION; // fragment world position
    float4 sv_pos : SV_Position; // fragment screen position
};

struct PsOut
{
    float4 col : SV_TARGET;
    float depth : SV_Depth;
};


#ifndef DISCARD_MISS
#define DISCARD_MISS 0
#endif

#ifdef ENABLE_DEBUG_UTILS
cbuffer debugCB
{
    uint2 screenResolution;
    bool saveDepthToDebugTexture;
    bool saveConvergence;
};
RWTexture2D<float> debugTexture;
RWStructuredBuffer<uint> debugBuffer;

void doDebugWrites(uint2 pixelCoord, TraceResult tr)
{
    if (saveDepthToDebugTexture)
    {
        debugTexture[pixelCoord].x = tr.T;
    }
    if (saveConvergence)
    {
        bool3 flags = bool3(tr.flags & (1u << 0), tr.flags & (1u << 1), tr.flags & (1u << 2));
        bool outOfIters = flags.z && !flags.y && !flags.x;
        bool convergedMiss = flags.x;
        bool convergedHit = flags.y;
        if(outOfIters)
            InterlockedAdd(debugBuffer[0], 1);
        if(convergedHit)
            InterlockedAdd(debugBuffer[1], 1);
        if (convergedMiss)
            InterlockedAdd(debugBuffer[2], 1);
    }
}
#else
void doDebugWrites(uint2 pixelCoord, TraceResult tr){ }
#endif


float3 iterToColor(uint iter, uint maxIter)
{
    float v = float(iter) / maxIter;
    const float ts[3 + 1] = { 0, 0.15, 0.4, 1.0 };
    const float3 Cs[3 + 1] = { float3(93, 127, 232) / 255., float3(92, 236, 220) / 255., float3(241, 222, 100) / 255., float3(220, 94, 75) / 255. };
    int i = 0;
    float3 a, b;
    for (; i < 3; ++i)
        if (ts[i + 1] >= v)
            break;
    return lerp(Cs[i], Cs[i + 1], (v - ts[i]) / (ts[i + 1] - ts[i]));
}

Ray getRay(float3 wPos)
{
    const float3 worldVec = wPos - camPos;
    const float wVecLen = length(worldVec);
    Ray ray;
    ray.orig = camPos;
    ray.dir = worldVec / wVecLen;
    ray.tMin = wVecLen;
    const Box box = { innerBoxCorner + 0.5 * innerBoxSize, 0.5 * innerBoxSize };
    if (!intersectBox(box, ray, false, ray.tMax))
    {
        ray.tMax = ray.tMin - 1;
    }
    return ray;
}

PsOut cube_main(PsIn psin, ITracer tracer)
{
    // get primary ray
    Ray ray = getRay(psin.pos);

    // primary trace
    SphereTraceDesc trD = { traceEpsilon, maxStep, stepRelaxation, false };
    TraceResult traceRes = tracer.trace(ray, trD);
    bool3 traceFlags = bool3(traceRes.flags & (1u << 0), traceRes.flags & (1u << 1), traceRes.flags & (1u << 2));
    traceFlags.z = traceFlags.z || (traceRes.flags & (1u << 3));
    
#if DISCARD_MISS
    // discard & early out
    if (!traceFlags.y && !traceFlags.z) {
        discard; PsOut o; return o;
    }
#endif
    
    // shade
    float3 col = shade(ray, traceRes.T, tracer, trD);
    
#if !(DISCARD_MISS)
    float3 debugCol = float3(traceFlags);
    if (!traceFlags.y)
        col = debugCol;
#endif

    // debug stuff
    doDebugWrites(uint2(psin.sv_pos.xy), traceRes);
    
    // return data
    PsOut o;
    o.col = float4(col, 1);
    
#if defined( ENABLE_DEBUG_UTILS ) && defined ( DEBUG_COLORING )
#if DEBUG_COLORING == 1
    // step count
    o.col = float4(iterToColor(stepCount, maxStep), 1);
#elif DEBUG_COLORING == 2
    // backstep count
    o.col = float4(iterToColor(backStep, maxStep/5), 1);
#elif DEBUG_COLORING == 3
    // avg step size
    o.col = lerp(float4(1,0,0,1), float4(1,1,0,1), (traceRes.T - ray.tMin) / stepCount * length(innerBoxSize) * 2);
#endif
#endif
    // calc fragment depth
    float4 depth_vec = mul(viewProj, float4(traceRes.T * ray.dir + ray.orig, 1));
    o.depth = depth_vec.z / depth_vec.w;
    
    return o;
}

// shader entry for SDF trace
PsOut main(PsIn psin)
{
    SDFTracer sdfTracer;
    // not really necessary but my syntax highlighter doesn't like sdfTracer as the parameter
    ITracer tr = sdfTracer;
    return cube_main(psin, tr);
}
