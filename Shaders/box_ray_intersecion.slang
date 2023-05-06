#ifndef BOX_RAY_INTERSECTION_SLANG_INCLUDED
#define BOX_RAY_INTERSECTION_SLANG_INCLUDED

#include "types.slang"

float maxx(float3 v)
{
    return max(max(v.x, v.y), v.z);
}

struct Box
{
    float3 center;
    float3 radius;
//	float3 invRadius;
//	float3x3 rot;
};
// based on http://www.jcgt.org/published/0007/03/04/paper-lowres.pdf
bool intersectBox(Box box, Ray ray, bool front, out float dist)
{
    ray.orig = ray.orig - box.center;
    float winding = front ? 1 : -1;
    float3 sgn = -sign(ray.dir);
    float3 d = box.radius * winding * sgn - ray.orig;
    d /= ray.dir;
#define TEST_BOX(U, VW) (d.U >= 0.0) && all(abs(ray.orig.VW + ray.dir.VW * d.U)< box.radius.VW)
    bool3 test = bool3(TEST_BOX(x, yz), TEST_BOX(y, zx), TEST_BOX(z, xy));
#undef TEST_BOX
    sgn = test.x ? float3(sgn.x, 0, 0) : (test.y ? float3(0, sgn.y, 0) : float3(0, 0, test.z ? sgn.z : 0));
    dist = (sgn.x != 0) ? d.x : ((sgn.y != 0) ? d.y : d.z);
    return (sgn.x != 0) || (sgn.y != 0) || (sgn.z != 0);
}

#endif
