float3 mod(float3 x, float y)
{
    return x - y * floor(x / y);
}

// [-2,2]^3
float funDist(float3 p)
{
    p = mod(p, 1.0);
    // sphere
    return length(p - 0.5) - .35;
}
