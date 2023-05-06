cbuffer PScb
{
    float3 color;
};
struct PsOut
{
    float4 col : SV_TARGET;
};

PsOut main()
{
    PsOut o;
    o.col = float4(color, 1);
    return o;
}
