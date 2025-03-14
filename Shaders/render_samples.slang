struct Sample
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct VsOut
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
};

struct GsOut
{
    float4 posH : SV_POSITION;
    float tex : TEXCOORD;
    float3 normal : NORMAL;
};

struct PsOut
{
    float4 col : SV_TARGET;
//    float depth : SV_Depth;
};


cbuffer GScb
{
    float4x4 MVP;
    float normalLength;
};

VsOut vs_main(Sample samp)
{
    VsOut s;
    s.normal = samp.normal;
    s.pos = float4(samp.pos, 1);
    return s;
}

[maxvertexcount(2)]
void gs_main(point VsOut samps[1], inout LineStream<GsOut> lineStream)
{
    VsOut samp = samps[0];
    float3 outPos2 = samp.pos.xyz + samp.normal * normalLength;
    
    GsOut outVert;
    outVert.normal = samp.normal;
    outVert.tex = 0.0;
    outVert.posH = mul(MVP, float4(samp.pos.xyz, 1));
    GsOut outVert2;
    outVert2.normal = samp.normal;
    outVert2.tex = 1.0;
    outVert2.posH = mul(MVP, float4(outPos2, 1));

    lineStream.Append(outVert);
    lineStream.Append(outVert2);
}

PsOut ps_main(GsOut psin)
{
    PsOut o;
    o.col = lerp(float4(0, 0, 0, 1), float4(0.5 + 0.5 * psin.normal, 1), smoothstep(0.2, 0.3, psin.tex));
    return o;
}
