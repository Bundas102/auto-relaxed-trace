#ifndef TYPES_SLANG_INCLUDED
#define TYPES_SLANG_INCLUDED

struct Ray
{
    float3 orig;
    float tMin;
    float3 dir;
    float tMax;
};

struct TraceResult
{
    float T; // Distance taken on ray
    uint flags; // bit 0:   distance condition:     1 if travelled too far t > t_max
                // bit 1:   surface condition:      1 if distance to surface is < error threshold
                // bit 2:   iteration condition:    1 if the trace took too many iterations
};


struct SphereTraceDesc
{
    float epsilon; // ray stopping distance to surface
    int maxiters;  // maximum iteration count
    float stepRelaxation; // relax sphere trace step
    bool shadowRay; // whether the trace is for a hard shadow or primary ray
};

#ifdef ENABLE_DEBUG_UTILS
static uint backStep = 0;
static uint stepCount = 0;
#endif

#endif
