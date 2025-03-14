cbuffer VScb
{
    float3 modelScale;
    float3 modelTrans;
    float4x4 viewProj;
};

static const float3 P[] =
{
    {0,0,0}, {0,0,1}, {0,1,0}, {0,1,1},
    {1,0,0}, {1,0,1}, {1,1,0}, {1,1,1}
};

static const uint I[] =
{
    0,1, 0,2, 0,4,
    1,3, 1,5,
    2,3, 2,6,
    3,7,
    4,5, 4,6,
    5,7,
    6,7
};

struct VsOut
{
    float3 posW : POSITION;
    float4 posH : SV_POSITION;
};

VsOut main(uint ID : SV_VertexID)
{
    VsOut o;
    const float3 vert = P[I[ID]];
    o.posW = vert * modelScale + modelTrans;
    o.posH = mul(viewProj, float4(o.posW, 1));
    return o;
}
