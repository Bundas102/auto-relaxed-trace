
cbuffer CScb
{
    uint2 maxSize;
    float3 color;
};

RWTexture2D<float4> tex;

[numthreads(16, 16, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    if (any(threadId.xy >= maxSize))
        return;
    float2 p = 2 * float2(threadId.xy + 0.5) / maxSize - 1;
    p *= float2(maxSize) / min(maxSize.x, maxSize.y);
     bool cond = (threadId.x/8 + threadId.y/8) % 2;
    //bool cond = length(p) < .95;
    float3 col = cond ? color : 1 - color;
    tex[threadId.xy] = float4(col, 1);
}
