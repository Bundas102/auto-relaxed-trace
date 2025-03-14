#ifndef TRACE_SLANG_INCLUDED
#define TRACE_SLANG_INCLUDED

#include "types.slang"
#include "sdf.slang"

#ifndef SDF_TRACE_FUN_NUM
#define SDF_TRACE_FUN_NUM 1
#endif

interface ITracer
{
    TraceResult trace(Ray ray, SphereTraceDesc params);
};

struct SDFTracer : ITracer
{
    TraceResult traceClassic(Ray ray, SphereTraceDesc params)
    {
        // trace in local model coordinates
        ray.orig -= outerBoxCorner;

        TraceResult ret = { ray.tMin, 0 };

        int i = 0;
        float dd = sdfInside(ray.orig + ret.T * ray.dir);
        float prevSign = dd;
        float prevT = ret.T;
        for (; i < params.maxiters && dd > params.epsilon && ret.T < ray.tMax; ++i)
        {
            prevT = ret.T;
            ret.T += dd;
            ret.T = min(ret.T, ray.tMax);
            prevSign = dd;
            dd = sdfInside(ray.orig + ret.T * ray.dir);
        }

        if (dd <= params.epsilon && !params.shadowRay)
        {
            // linear approx == f(t) = f0 + t*(f1-f0) to reconstruct at t = 0 and t = 1
            float f0 = prevSign;
            float f1 = dd;
            float t = (f0 - f1 == 0.0) ? 1.0 : f0 / (f0 - f1);
            ret.T = lerp(prevT, ret.T, t);
            dd = lerp(f0, f1, t);
        }
        
#ifdef ENABLE_DEBUG_UTILS
        backStep = 0;
        stepCount = i;
#endif
        ret.flags = uint(ret.T >= ray.tMax)
              | (uint(abs(dd) <= params.epsilon) << 1)
              | (uint(i >= params.maxiters) << 2);
    
        return ret;
    }
    
    TraceResult traceRelaxed(Ray ray, SphereTraceDesc params)
    {
        ray.orig -= outerBoxCorner; // trace in local model coordinates
        TraceResult ret = { ray.tMin, 0 };
        int i = 0;
        float di = 0., ri = 0., ri1 = 0.;
        do
        {
            di = ri * (di == 0. ? 1. : params.stepRelaxation); //if d==0 we are stepping back
            ri1 = sdfInside(ray.orig + (ret.T + di) * ray.dir); //single sdf eval at t + di
            ++i;
            if (di > ri + abs(ri1))
            { // normal step can only occur after enhanced because di==ri when normal step
                di = 0.; //normal step next cycle
#ifdef ENABLE_DEBUG_UTILS
                backStep++;
#endif
            }
            else
            { // rotate variables when relaxed stepping
                ri = ri1;
            }
            ret.T += di;
        } while (ret.T < ray.tMax               // miss
                && ri > params.epsilon  // hit
                && i < params.maxiters); // didn't converge
#ifdef ENABLE_DEBUG_UTILS
        stepCount = i;
#endif
        ret.T = min(ret.T, ray.tMax);
        ret.flags = (int(ret.T >= ray.tMax) << 0) // miss
              | (int(ri <= params.epsilon) << 1) // hit
              | (int(i >= params.maxiters) << 2); // didn't converge
        return ret;
    }

    float enhanceSphereTraceStep(float di, float ri0, float ri, float relax)
    {
        return relax * ri * (di - ri0 + ri) / max(di + ri0 - ri, 0.00001);
    }
    
    TraceResult traceEnhanced(Ray ray, SphereTraceDesc params)
    {
        ray.orig -= outerBoxCorner; // trace in local model coordinates
        
        TraceResult ret = { ray.tMin, 0 };
        int i = 0;
        float di = 0., ri0 = 0., ri = 0., ri1 = 0.;

        do
        {
            di = ri + (di == 0. ? 0. : enhanceSphereTraceStep(di, ri0, ri, params.stepRelaxation)); //if d==0 we are stepping back
        
            ri1 = sdfInside(ray.orig + (ret.T + di) * ray.dir); //single sdf eval at t + di
            ++i;
        
            if (di > ri + abs(ri1))
            { // normal step can only occur after enhanced because di==ri when normal step
                di = 0.; //normal step next cycle
#ifdef ENABLE_DEBUG_UTILS
                backStep++;
#endif
            }
            else
            { // rotate variables when enhanced stepping
                ri0 = ri;
                ri = ri1;
            }
            ret.T += di;
        } while (ret.T < ray.tMax               // miss
                && ri > params.epsilon          // hit
                && i < params.maxiters); // didn't converge

#ifdef ENABLE_DEBUG_UTILS
        stepCount = i;
#endif
        ret.T = min(ret.T, ray.tMax);
        ret.flags = (int(ret.T >= ray.tMax) << 0) // miss
              | (int(ri <= params.epsilon) << 1) // hit
              | (int(i >= params.maxiters) << 2); // didn't converge
        return ret;
    }

    float calcSlope(float t0, float t1, float r0, float r1)
    {
        return ( r1 - r0 ) / max(t1 - t0, 1e-5);
    }
    TraceResult traceAutoRelaxation(Ray ray, SphereTraceDesc params)
    {
        ray.orig -= outerBoxCorner; // trace in local model coordinates
        
        float t = ray.tMin;
        float r = sdfInside(ray.orig + t * ray.dir);
        int i = 1;
        float z = r;
        float m = -1;
        while (t + r < ray.tMax          // miss
                && r > params.epsilon    // hit
                && i < params.maxiters)  // didn't converge
        {
            float T = t + z;
            float R = sdfInside(ray.orig + T * ray.dir);
            bool doBackStep = z > abs(R) + r;
            // bool doBackStep = t + abs(r) < T - abs(R);
            float M = calcSlope(t, T, r, R);
            m = doBackStep ? -1 : lerp(m, M, params.stepRelaxation);
            t = doBackStep ? t : T;
            r = doBackStep ? r : R;
            float omega = max(1.0, 2.0 / (1.0 - m));
            z = max(params.epsilon, r * omega);
            ++i;
#ifdef ENABLE_DEBUG_UTILS
            backStep += doBackStep ? 1 : 0;
#endif
        }
            
#ifdef ENABLE_DEBUG_UTILS
        stepCount = i;
#endif
        TraceResult ret;
        ret.T = t + r;
        ret.T = min(ret.T, ray.tMax);
        ret.flags = (int(ret.T >= ray.tMax) << 0) // miss
              | (int(r <= params.epsilon) << 1)   // hit
              | (int(i >= params.maxiters) << 2); // didn't converge
        return ret;
    }
    
    TraceResult trace(Ray ray, SphereTraceDesc params)
    {
#if SDF_TRACE_FUN_NUM == 1
        return traceClassic(ray, params);
#elif SDF_TRACE_FUN_NUM == 2
        return traceRelaxed(ray, params);
#elif SDF_TRACE_FUN_NUM == 3
        return traceEnhanced(ray, params);
#elif SDF_TRACE_FUN_NUM == 4
        return traceAutoRelaxation(ray, params);
#else
#error Unkown value for SDF_TRACE_FUN_NUM
#endif
    }
};

#endif
