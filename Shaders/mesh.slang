#ifndef MESH_SLANG_INCLUDED
#define MESH_SLANG_INCLUDED

#ifndef MESH_CHUNK_SIZE
#define MESH_CHUNK_SIZE 8192
#endif

struct MeshBufferTriangle {
    float aX, aY, aZ;
    float bX, bY, bZ;
    float cX, cY, cZ;
};
struct Triangle {
    float3 a, b, c;
};
StructuredBuffer<MeshBufferTriangle> triangleBuffer;

Triangle getTriangle(uint index)
{
    MeshBufferTriangle t = triangleBuffer[index];
    Triangle ret;
    ret.a = float3(t.aX, t.aY, t.aZ);
    ret.b = float3(t.bX, t.bY, t.bZ);
    ret.c = float3(t.cX, t.cY, t.cZ);
    return ret;
}


struct MeshCalcData{
    float SDFsq; // smallest distance to triangles (squared)
    uint3 intersections; // number of intersections with triangles in the cardinal directions (mod 2)
};
MeshCalcData initMeshCalcData()
{
    MeshCalcData ret;
    ret.SDFsq = 65504.0;
    ret.intersections = uint3(0);
    return ret;
}
float4 encodeMeshCalcData(MeshCalcData d){
    uint3 ii = d.intersections % 2;
    uint iii = (ii.x << 0) + (ii.y << 1) + (ii.z << 2);
    return float4(d.SDFsq, iii, 0, 0);
}
MeshCalcData decodeMeshCalcData(float4 d){
    MeshCalcData ret;
    ret.SDFsq = d.x;
    uint iii = d.y;
    ret.intersections = uint3(iii % 2, (iii % 4) >> 1, (iii % 8) >> 2);
    return ret;
}

float dot2(in float3 v)
{
    return dot(v, v);
}
int rayTriangleIntersect(float3 orig, float3 dir, Triangle tt)
{
    // based on Müller-Trumbore's algorithm
    float3 v0 = tt.a, v1 = tt.b, v2 = tt.c;
    const float EPS = 0.00001;
    float3 e1 = v1 - v0;
    float3 e2 = v2 - v0;
    float3 h = cross(dir, e2);
    float a = dot(e1, h);
    if (abs(a) < EPS)
        return 0;
    float f = 1 / a;
    float3 s = orig - v0;
    float u = f * dot(s, h);
    if (u < 0 || u > 1)
        return 0;
    float3 q = cross(s, e1);
    float v = f * dot(dir, q);
    if (v < 0 || u + v > 1)
        return 0;
    float t = f * dot(e2, q);
    return t > EPS ? 1 : 0;
}

float triangleDistSquared(float3 p, Triangle t)
{
    // IQ's triangle distance function
    float3 a = t.a, b = t.b, c = t.c;
    float3 ba = b - a; float3 pa = p - a;
    float3 cb = c - b; float3 pb = p - b;
    float3 ac = a - c; float3 pc = p - c;
    float3 nor = cross(ba, ac);

    return
        (sign(dot(cross(ba, nor), pa)) +
            sign(dot(cross(cb, nor), pb)) +
            sign(dot(cross(ac, nor), pc)) < 2.0)
        ?
        min(min(
            dot2(ba * clamp(dot(ba, pa) / dot2(ba), 0.0, 1.0) - pa),
            dot2(cb * clamp(dot(cb, pb) / dot2(cb), 0.0, 1.0) - pb)),
            dot2(ac * clamp(dot(ac, pc) / dot2(ac), 0.0, 1.0) - pc))
        :
        dot(nor, pa) * dot(nor, pa) / dot2(nor);
}

MeshCalcData processMeshChunk(uint3 chunkOffset, float3 evalPos, MeshCalcData data, float3 meshCenter, float3 cellSize)
{
    MeshCalcData d = data;
    
    float3 rayX = evalPos.x > meshCenter.x ? float3(1, 0, 0) : float3(-1, 0, 0);
    float3 rayY = evalPos.y > meshCenter.y ? float3(0, 1, 0) : float3(0, -1, 0);
    float3 rayZ = evalPos.z > meshCenter.z ? float3(0, 0, 1) : float3(0, 0, -1);
    
    const uint maxIndex = min(chunkOffset.x + MESH_CHUNK_SIZE, chunkOffset.y);
    for (uint i = chunkOffset.x; i < maxIndex; i++)
    {
        Triangle t = getTriangle(i);
        
        float value = triangleDistSquared(evalPos, t);
            
        d.intersections.x += rayTriangleIntersect(evalPos, rayX, t);
        d.intersections.y += rayTriangleIntersect(evalPos, rayY, t);
        d.intersections.z += rayTriangleIntersect(evalPos, rayZ, t);
        
        d.SDFsq = min(d.SDFsq, value);
    }
    
    return d;
}


#endif
