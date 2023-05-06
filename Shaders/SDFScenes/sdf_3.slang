#include "primitives.slang"

// Start of generated code
float SceneSDF(float3 p)
{
	float r0 = box(p, float3(0.5,0.3,0.5));
	float r1 = cylinderY(p - float3(-0.5,0,-0.5), float2(0.2,0.6));
	r1 *= -1.f; //invert
	r0 = max(r0,r1); //intersect 1
	float r2 = cylinderY(p - float3(0.5,0,-0.5), float2(0.2,0.6));
	r2 *= -1.f; //invert
	r0 = max(r0,r2); //intersect 1
	float r3 = cylinderY(p - float3(-0.5,0,0.5), float2(0.2,0.6));
	r3 *= -1.f; //invert
	r0 = max(r0,r3); //intersect 1
	float r4 = cylinderY(p - float3(0.5,0,0.5), float2(0.2,0.6));
	r4 *= -1.f; //invert
	r0 = max(r0,r4); //intersect 1
	float r5 = cylinderY(p - float3(-0.22,0,-0.42), float2(0.04,0.6));
	r5 *= -1.f; //invert
	r0 = max(r0,r5); //intersect 1
	float r6 = cylinderY(p - float3(0.42,0,-0.22), float2(0.04,0.6));
	r6 *= -1.f; //invert
	r0 = max(r0,r6); //intersect 1
	float r7 = cylinderY(p - float3(-0.42,0,0.22), float2(0.04,0.6));
	r7 *= -1.f; //invert
	r0 = max(r0,r7); //intersect 1
	float r8 = cylinderY(p - float3(0.22,0,0.42), float2(0.04,0.6));
	r8 *= -1.f; //invert
	r0 = max(r0,r8); //intersect 1
	float r9 = cylinderY(p - float3(0,0.5,0), float2(0.3,0.12));
	float r10 = cylinderY(p - float3(0,0.32,0), float2(0.3,0.02));
	r9 = min(r9,r10); //union 1
	float r11 = cylinderY(p - float3(0,0.68,0), float2(0.3,0.02));
	r9 = min(r9,r11); //union 2
	float r12 = cylinderY(p - float3(0,0.5,0), float2(0.255,0.2));
	r9 = min(r9,r12); //union 3
	float r13 = cylinderY(p - float3(0,0.64,0), float2(0.18,0.064));
	r13 *= -1.f; //invert
	r9 = max(r9,r13); //intersect 1
	float r14 = cylinderY(p - float3(0,0.36,0), float2(0.18,0.064));
	r14 *= -1.f; //invert
	r9 = max(r9,r14); //intersect 1
	float r15 = cylinderY(p - float3(0,0.5,0), float2(0.126,0.2));
	r15 *= -1.f; //invert
	r9 = max(r9,r15); //intersect 1
	r0 = min(r0,r9); //union 1
return r0;
}

// End of generated code

float funDist(float3 p)
{
	return SceneSDF(p);
}
