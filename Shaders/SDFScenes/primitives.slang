#define SQRT2 1.4142135623
#define PI 3.14159265359
#define PI2 (2*3.14159265359)

//#pragma warning(disable : 4008)


#define INF 3.402823466e+38
#define CONVEXOPT_FPOS(f, vdf) float2(f, (vdf < 0.0 ? -f/vdf : INF) )
#define CONVEXOPT(f, vdf) float2(f, f < 0.0 ? f : (vdf < 0.0 ? -f/vdf : INF) )

// Planes
float plane(float3 p, float3 n)
{
    return dot(p, normalize(n));
}
// Axis aligned planes
float planeYZ(float3 p){return p.x;}
float planeXZ(float3 p){return p.y;}
float planeXY(float3 p){ return p.y;}

// Sphere
float sphere(float3 p, float r)
{
    return length(p) - r;
}

// Infinite cylinders
float cylinderZ(float3 p, float r)
{
    return length(p.xy) - r;
}
float  cylinderX(float3 p, float r){return cylinderZ(p.zyx,r);}
float  cylinderY(float3 p, float r){return cylinderZ(p.xzy,r);}

// Finite cylinders
float cylinderZ(float3 p, float2 h)
{
    float2 d = abs(float2(length(p.xy), abs(p.z))) - h;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
float  cylinderX(float3 p, float2 h){return cylinderZ(p.zyx,h);}
float  cylinderY(float3 p, float2 h){return cylinderZ(p.xzy,h);}

// Box
float box(float3 p, float3 size)
{
    float3 d = abs(p) - size;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}
// ************************
//      SET OPERATIONS
// ________________________

float Offset(float f, float r){return f - r;}

float Union(float d1, float d2){ return min(d1,d2);}

float Intersect(float d1, float d2){ return max(d1,d2);}

float Substract(float d1, float d2){return max(d1,-d2);}
