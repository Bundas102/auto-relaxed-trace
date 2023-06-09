/*
Copyright 2016 Thomas Hooper @tdhooper
License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
Link: https://www.shadertoy.com/view/MlcGRf
*/

/******************************************************************************
 This work is a derivative of work by Thomas Hooper used under CC BY-NC-SA 3.0.
 This work is licensed also under CC BY-NC-SA 3.0 by NVIDIA CORPORATION.
 ******************************************************************************/

#ifndef dodeca_slang
#define dodeca_slang

#define PHI (1.618033988749895)

#define GDFVector3 normalize(float3(1, 1, 1 ))
#define GDFVector4 normalize(float3(-1, 1, 1))
#define GDFVector5 normalize(float3(1, -1, 1))
#define GDFVector6 normalize(float3(1, 1, -1))

#define GDFVector7 normalize(float3(0, 1, PHI+1.))
#define GDFVector8 normalize(float3(0, -1, PHI+1.))
#define GDFVector9 normalize(float3(PHI+1., 0, 1))
#define GDFVector10 normalize(float3(-PHI-1., 0, 1))
#define GDFVector11 normalize(float3(1, PHI+1., 0))
#define GDFVector12 normalize(float3(-1, PHI+1., 0))

#define GDFVector13 normalize(float3(0, PHI, 1))
#define GDFVector14 normalize(float3(0, -PHI, 1))
#define GDFVector15 normalize(float3(1, 0, PHI))
#define GDFVector16 normalize(float3(-1, 0, PHI))
#define GDFVector17 normalize(float3(PHI, 1, 0))
#define GDFVector18 normalize(float3(-PHI, 1, 0))

#define fGDFBegin float d = 0.;
#define fGDF(v) d = max(d, abs(dot(p, v)));
#define fGDFEnd return d - r;

float fDodecahedron(float3 p, float r) {
	fGDFBegin
	fGDF(GDFVector13) fGDF(GDFVector14) fGDF(GDFVector15) fGDF(GDFVector16)
	fGDF(GDFVector17) fGDF(GDFVector18)
	fGDFEnd
}

float funDist(float3 p) {
	const float scale = 0.7;
	p *= 1./scale;
	return fDodecahedron(p, 1.0) * scale;
}

#endif
