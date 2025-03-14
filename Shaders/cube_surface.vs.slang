cbuffer VScb
{
    float3 modelScale;
    float3 modelTrans;
    float4x4 viewProj;
    float4x4 inverseViewProj;
    float3 cameraPos;
    float3 cameraDir;
    float planeDist;
    uint type;
};

#include "box_plane_intersecion.slang"

// unit cube: triangle_strip with 14 vertices
// X: 10 1000 0111 1010  = 0x287a
// Y: 00 0010 1010 1111  = 0x2af
// Z: 11 0001 1110 0011  = 0x31e3
float3 unit_cube(uint vert_id)
{
    return float3((uint3(0x287a, 0x2af, 0x31e3) >> vert_id) & 1);
}

// full screen "quad": triangle with 3 vertices
float4 full_screen_quad(uint vert_id)
{
    static const float2 pos[3] = { float2(-1, -1), float2(3, -1), float2(-1, 3) };
    return float4(pos[vert_id], 0, 1);
}

struct VsOut
{
    float3 posW : POSITION;
    float4 posH : SV_POSITION;
};

VsOut main(uint ID : SV_VertexID)
{
    VsOut o;
    if (type == 2)
    { // cube
        float3 vert = unit_cube(ID);
        o.posW = vert * modelScale + modelTrans;
        o.posH = mul(viewProj, float4(o.posW, 1));
    }
    else if (type == 1)
    { // clip
        BBox box = { modelScale, modelTrans };
        Plane plane = { cameraDir, planeDist };
        o.posW = box_plane_intersection(ID, box, plane);
        o.posH = mul(viewProj, float4(o.posW, 1));
        o.posH.z = 0;
    }
    else
    { // quad
        o.posH = full_screen_quad(ID);
        float4 p = mul(inverseViewProj, o.posH);
        o.posW = p.xyz / p.w;
    }
    return o;
}
