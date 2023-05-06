/*
Copyright 2020 @Maurogik
License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
Link: https://www.shadertoy.com/view/tdjyzz
*/


/******************************************************************************
 This work is a derivative of work by Maurogik used under CC BY-NC-SA 3.0.
 This work is licensed also under CC BY-NC-SA 3.0 by NVIDIA CORPORATION.
 ******************************************************************************/

#ifndef tree_glsl
#define tree_glsl

//   __  _                            _                   _       
//  / _\| |__    __ _  _ __  ___   __| |   ___  ___    __| |  ___ 
//  \ \ | '_ \  / _` || '__|/ _ \ / _` |  / __|/ _ \  / _` | / _ \
//  _\ \| | | || (_| || |  |  __/| (_| | | (__| (_) || (_| ||  __/
//  \__/|_| |_| \__,_||_|   \___| \__,_|  \___|\___/  \__,_| \___|
//                                                                

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SDF functions from mercury : http://mercury.sexy/hg_sdf/ //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//#define saturate(x) clamp(x, 0.0, 1.0)
#define PI 3.14159265359

float mod(float x, float y)
{
    return x - y * floor(x / y);
}
float2 mod(float2 x, float2 y)
{
    return x - y * floor(x / y);
}
float3 mod(float3 x, float3 y)
{
    return x - y * floor(x / y);
}

////////////////////////////////////////////////////////////////
//
//             PRIMITIVE DISTANCE FUNCTIONS
//
////////////////////////////////////////////////////////////////
//
// Conventions:
//
// Everything that is a distance function is called fSomething.
// The first argument is always a point in 2 or 3-space called <p>.
// Unless otherwise noted, (if the object has an intrinsic "up"
// side or direction) the y axis is "up" and the object is
// centered at the origin.
//
////////////////////////////////////////////////////////////////

float smin( float a, float b, float k ) {
    float h = clamp( 0.5+0.5*(b-a)/k, 0., 1. );
    return lerp( b, a, h ) - k*h*(1.0-h);
}

float smax(float a, float b, float k) {
    return smin(a, b, -k);
}

float sdCone( in float3 p, in float3 c )
{
    float2 q = float2(length(p.xz), p.y );
    float d1 = -q.y - c.z;
    float d2 = max(dot(q,c.xy), q.y);
    return length(max(float2(d1,d2),0.0)) + min(max(d1,d2), 0.);
}

float fSphere(float3 p, float r)
{
    return length(p) - r;
}

// Capsule: A Cylinder with round caps on both sides
float fCapsule(float3 p, float r, float c)
{
    return lerp(length(p.xz) - r, length(float3(p.x, abs(p.y) - c, p.z)) - r, step(c, abs(p.y)));
}

// Distance to line segment between <a> and <b>, used for fCapsule() version 2below
float fLineSegment(float3 p, float3 a, float3 b)
{
    float3  ab = b - a;
    float t  = saturate(dot(p - a, ab) / dot(ab, ab));
    return length((ab * t + a) - p);
}

// Capsule version 2: between two end points <a> and <b> with radius r
float fCapsule(float3 p, float3 a, float3 b, float r)
{
    return fLineSegment(p, a, b) - r;
}

// Cone with correct distances to tip and base circle. Y is up, 0 is in the middle of the base.
float fCone(float3 p, float radius, float height)
{
    float2  q         = float2(length(p.xz), p.y);
    float2  tip       = q - float2(0.0, height);
    float2  mantleDir = normalize(float2(height, radius));
    float mantle    = dot(tip, mantleDir);
    float d         = max(mantle, -q.y);
    float projected = dot(tip, float2(mantleDir.y, -mantleDir.x));

    // distance to tip
    if((q.y > height) && (projected < 0.0))
    {
        d = max(d, length(tip));
    }

    // distance to base ring
    if((q.x > radius) && (projected > length(float2(height, radius))))
    {
        d = max(d, length(q - float2(radius, 0.0)));
    }
    return d;
}


////////////////////////////////////////////////////////////////
//
//                DOMAIN MANIPULATION OPERATORS
//
////////////////////////////////////////////////////////////////
//
// Conventions:
//
// Everything that modifies the domain is named pSomething.
//
// Many operate only on a subset of the three dimensions. For those,
// you must choose the dimensions that you want manipulated
// by supplying e.g. <p.x> or <p.zx>
//
// <inout p> is always the first argument and modified in place.
//
// Many of the operators partition space into cells. An identifier
// or cell index is returned, if possible. This return value is
// intended to be optionally used e.g. as a random seed to change
// parameters of the distance functions inside the cells.
//
// Unless stated otherwise, for cell index 0, <p> is unchanged and cells
// are centered on the origin so objects don't have to be moved to fit.
//
//
////////////////////////////////////////////////////////////////


// Rotate around a coordinate axis (i.e. in a plane perpendicular to that axis) by angle <a>.
// Read like this: R(p.xz, a) rotates "x towards z".
// This is fast if <a> is a compile-time constant and slower (but still practical) if not.
void pR(inout float2 p, float a)
{
    p = cos(a) * p + sin(a) * float2(p.y, -p.x);
}

// Repeat around the origin by a fixed angle.
// For easier use, num of repetitions is use to specify the angle.
float pModPolar(inout float2 p, float repetitions)
{
    float angle = 2.0 * PI / repetitions;
    float a     = atan2(p.y, p.x) + angle / 2.;
    float r     = length(p);
    float c     = floor(a / angle);
    a           = mod(a, angle) - angle / 2.;
    p           = float2(cos(a), sin(a)) * r;
    // For an odd number of repetitions, fix cell index of the cell in -x direction
    // (cell index would be e.g. -5 and 5 in the two halves of the cell):
    if(abs(c) >= (repetitions / 2.0))
        c = abs(c);
    return c;
}

// Repeat in two dimensions
float2 pMod2(inout float2 p, float2 size)
{
    float2 c = floor((p + size * 0.5) / size);
    p      = mod(p + size * 0.5, size) - size * 0.5;
    return c;
}

// Repeat in three dimensions
float3 pMod3(inout float3 p, float3 size)
{
    float3 c = floor((p + size * 0.5) / size);
    p      = mod(p + size * 0.5, size) - size * 0.5;
    return c;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Intersectors and other things from IQ
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// http://iquilezles.org/www/articles/smin/smin.htm
/*float smin( float a, float b, float k )
{
    float h = max(k-abs(a-b),0.0);
    return min(a, b) - h*h*0.25/k;
}

// http://iquilezles.org/www/articles/smin/smin.htm
float smax( float a, float b, float k )
{
    float h = max(k-abs(a-b),0.0);
    return max(a, b) + h*h*0.25/k;
}


// vertical
float sdCone( in float3 p, in float3 c )
{
    float2 q = float2( length(p.xz), p.y );
    float d1 = -q.y-c.z;
    float d2 = max( dot(q,c.xy), q.y);
    return length(max(float2(d1,d2),0.0)) + min(max(d1,d2), 0.);
}*/

float sdOctogon( in float2 p, in float r )
{
    const float3 k = float3(-0.9238795325, 0.3826834323, 0.4142135623 );
    p = abs(p);
    p -= 2.0*min(dot(float2( k.x,k.y),p),0.0)*float2( k.x,k.y);
    p -= 2.0*min(dot(float2(-k.x,k.y),p),0.0)*float2(-k.x,k.y);
    p -= float2(clamp(p.x, -k.z*r, k.z*r), r);
    return length(p)*sign(p.y);
}

// plane degined by p (p.xyz must be normalized)
float plaIntersect( in float3 ro, in float3 rd, in float4 p )
{
    return -(dot(ro,p.xyz)+p.w)/dot(rd,p.xyz);
}

float sdTriangleIsosceles( in float2 p, in float2 q )
{
    p.x = abs(p.x);
    float2 a = p - q*clamp( dot(p,q)/dot(q,q), 0.0, 1.0 );
    float2 b = p - q*float2( clamp( p.x/q.x, 0.0, 1.0 ), 1.0 );
    float s = -sign( q.y );
    float2 d = min( float2( dot(a,a), s*(p.x*q.y-p.y*q.x) ),
                  float2( dot(b,b), s*(p.y-q.y)  ));
    return -sqrt(d.x)*sign(d.y);
}

float opExtrusion( in float3 p, in float dist, in float h )
{
    float2 w = float2( dist, abs(p.z) - h );
    return min(max(w.x,w.y),0.0) + length(max(w,0.0));
}

//From https://www.iquilezles.org/www/articles/sphereshadow/sphereshadow.htm
float sphSoftShadow( in float3 ro, in float3 rd, in float3 sph, in float ra, in float k )
{
    float3 oc = ro - sph.xyz;
    float b = dot( oc, rd );
    float c = dot( oc, oc ) - ra*ra;
    float h = b*b - c;

    return (b>0.0) ? step(-0.0001,c) : smoothstep( -0.5, 0.5, h*k/b );
}

float3 opCheapBend( in float3 p, float bend )
{
    float k = bend;
    float c = cos(k*p.x);
    float s = sin(k*p.x);
    float2x2  m = float2x2(c,-s,s,c);
    float3 q = float3(mul(p.xy, m), p.z);
    return q;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hashes from Dave Hopkins 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Hash without Sine
// Creative Commons Attribution-ShareAlike 4.0 International Public License
// Created by David Hoskins.

//----------------------------------------------------------------------------------------
//  1 out, 1 in...
float tree_hash11(float p)
{
    p = frac(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

//----------------------------------------------------------------------------------------
//  1 out, 2 in...
float tree_hash12(float2 p)
{
    float3 p3  = frac(float3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

//----------------------------------------------------------------------------------------
//  1 out, 3 in...
float hash13(float3 p3)
{
    p3  = frac(p3 * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}


//----------------------------------------------------------------------------------------
///  2 out, 2 in...
float2 hash22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx+33.33);
    return frac((p3.xx+p3.yz)*p3.zy);

}

//----------------------------------------------------------------------------------------
///  3 out, 2 in...
float3 hash32(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz+33.33);
    return frac((p3.xxy+p3.yzz)*p3.zyx);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Common Shader Code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define NON_CONST_ZERO (min(iFrame,0)) 
#define NON_CONST_ZERO_U uint(min(iFrame,0)) 

static const float2 oz = float2(1.0, 0.0);

static const float kGoldenRatio = 1.618;
static const float kGoldenRatioConjugate = 0.618;

static const float kPI         = 3.14159265359;
static const float kTwoPI      = 2.0 * kPI;

static const float kMaxDist = 10000.;
static const float kTimeScale = 1.0;

static const float kIsotropicScatteringPhase = (1.0 / (4.0 * kPI));

float3 roughFresnel(float3 f0, float cosA, float roughness)
{
    // Schlick approximation
    return f0 + (oz.xxx - f0) * (pow(1.0 - cosA, 5.0)) * (1.0 - roughness);
}

float linearstep(float start, float end, float x)
{
    float range = end - start;
    return saturate((x - start) / range);
}

float henyeyGreensteinPhase_schlick(float cosA, float g)
{
    float k = 1.55*g - 0.55*g*g*g;
    float f = 1.0 - k * cosA;
    return (1.0 - k * k) / (4.0 * kPI * f*f);
}

float henyeyGreensteinPhase(float cosA, float g)
{
    return henyeyGreensteinPhase_schlick(cosA, g);
    /*float g2 = g*g;
    return 1.0 / (4.0 * kPI) *
        ((1.0 - g2)/pow(1.0 + g2 - 2.0*g*cosA, 1.5));*/
}

float rayleighPhase(float rayDotSun)
{
    float k = (3.0 / 4.0) * kIsotropicScatteringPhase;
    return k * (1.0 + rayDotSun * rayDotSun);
}

float rbgToluminance(float3 rgb)
{
    return (rgb.r * 0.3) + (rgb.g * 0.59) + (rgb.b * 0.11);
}

float3 fixNormalBackFacingness(float3 rayDirWS, float3 normalWS)
{
    normalWS -= max(0.0, dot(normalWS, rayDirWS)) * rayDirWS;
    return normalWS;
}

//This is NOT a good way of converting from wavelgnth to RGB
float3 wavelengthToRGB(float wavelength)
{
    const float kLambdaR = 680.0;
    const float kLambdaG = 550.0;
    const float kLambdaB = 440.0;
    
    float3 colour = oz.xxx - saturate(float3(abs(wavelength-kLambdaR), abs(wavelength-kLambdaG), abs(wavelength-kLambdaB))/150.0);
    return colour;  
}


#define USE_SPHERE 0
#define USE_TUBE 0
#define USE_SKY 1
#define USE_ATM 1
#define USE_SUN 1


static const float kSunRadius = 1.0/180.0*kPI;
static const float kCosSunRadius = cos(kSunRadius);
static const float kTanSunRadius = tan(kSunRadius);
static const float kSunDiskSolidAngle = 2.0*kPI*(1.0 - kCosSunRadius);

static const float kAtmDensity = 1.0;

static const float3 kRayleighScatteringCoefsKm = float3(5.8e-3, 1.35e-2, 3.31e-2) * kAtmDensity;
static const float kRayleighAtmHeightKm = 8.0;

static const float kMieAtmHeightKm = 1.2;
static const float kMieScatteringCoefsKm = 0.0075 * kAtmDensity * 2.0;

static const float kEarthRadiusKm = 6000.0;

static const float kMultipleScatteringFakery = 0.5;


static const float3 s_dirToSunWS = normalize(float3(0.4, -0.01, 0.5));
static const float3 s_sunColour = oz.yyy;
static const float3 s_cloudSunColour = oz.yyy;
static const float3 s_sunRadiance  = oz.yyy;
static const float3 s_averageSkyColour = oz.yyy;
static const float s_timeOfDay = 0.0f;
static const float s_time = 0.0f; // g3d_SceneTime;
static const float s_earthRotationRad = 0.0f;
static const float3 s_eyePositionWS = oz.xxx;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared SDF
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const float kMatMapleLeaf = 0.0;
static const float kMatFallingMapleLeaf = 1.0;
static const float kMatMapleBark = 2.0;
static const float kMatGrass = 3.0;
static const float kMatMountains = 4.0;
static const float kMatPines = 5.0;

static const uint kRenderFilter = 0u;
static const uint kShadowMapFilter = 1u;

static const float3 kWindVelocityWS = -oz.xyy * 0.75;

static const float3 kTreePosWS = float3(0.0, 0.0, -8.0);

//2 triangles and some displacement
float fMapleLeaf(float3 posLeaf, float scale, float rand, bool doDetail, out float4 material)
{
    posLeaf = opCheapBend(posLeaf.xzy, (rand-0.5) * 10.0).xzy;
    
    float normalisedHeight = saturate(0.5 + (posLeaf.y/scale));
    material = float4(kMatMapleLeaf, 1.0, 0.0, 0.0);
    
    float3 posTri = posLeaf - oz.yxy * scale;
    posTri.y = -posTri.y;
    
    if(doDetail)
    {
        posTri.y += max(0.0, cos(abs(posTri.x)*6.0*kPI)) * 0.035;
    }

    float pointyBits = 0.0;
    float dist2D = sdTriangleIsosceles(posTri.xy, float2(scale*1.2, scale*2.0));
    float innerStick = linearstep(0.01, 0.0, abs(posTri.x));
    innerStick = max(innerStick, linearstep(0.02, 0.0, abs(abs(posTri.x)-1.75*(posTri.y-1.4*scale))));
        
    if(doDetail)
    {
        pointyBits = abs(frac((-0.08 + posTri.y + abs(posTri.x)*2.0)*8.0) - 0.5) * 0.1 
            * saturate(1.0 - abs(-posTri.y/scale + 0.4)*1.);

        dist2D -= pointyBits;
    }
    
    float distToMid = (1.0 - 2.0*abs(normalisedHeight - 0.2));
    dist2D += distToMid*0.02;

    posTri.y -= scale * 0.75;
    posTri.x = abs(posTri.x) - scale * 1.3;
    pR(posTri.xy,  kPI * 0.35);
    
    float distTriSides = sdTriangleIsosceles(posTri.xy, float2(scale*0.55, scale*1.3));
    innerStick = max(innerStick, linearstep(0.01, 0.0, abs(posTri.x)));
    if(doDetail)
    {    
        pointyBits = abs(frac((-0.03 + posTri.y + abs(posTri.x)*2.0)*8.0) - 0.5) * 0.1 
            * saturate(1.0 - abs(-posTri.y/scale + 0.35)*2.5);
        distTriSides -= pointyBits;
    }
    dist2D = min(dist2D, distTriSides);
    
    float inside = max(0.0, -dist2D/scale);
    material.z = inside;
    material.w = floor(rand * 100.0) + innerStick*0.99;
    
    if(doDetail)
    {
        dist2D -= abs(frac((posLeaf.y/scale)*5.0)-0.5) * 0.02;
    }
    
    float minDist = opExtrusion(posLeaf, dist2D, 0.005);
    
    if(doDetail)
    {
        float3 posStick = posLeaf + oz.yxy * scale * 0.5;
        float stickProgress = saturate(-posStick.y/scale);
        posStick.z += stickProgress*stickProgress*scale*0.3*(rand - 0.5);
        float stickDist = fCapsule(posStick, -oz.yxy * scale * 1.0, oz.yyy, 0.003);
        if(stickDist < minDist)
        {
            minDist = stickDist;
        }        
    }
    
    return minDist;
}

float fBranchSDF(float3 posBranch, float len, float rad, float rand, out float4 material)
{
    float branchHalfLen = len * 0.5;
    float progressAlong = posBranch.y / (2.0*branchHalfLen);    
    float branchRad = rad * (1.0 - progressAlong * 0.8);
    
    float wave = sin((rand + posBranch.y)/len * 12.0)*0.25*rad;
    posBranch.xz += oz.xx * wave;
    float minDist = fCapsule(posBranch - oz.yxy * branchHalfLen, branchRad, branchHalfLen);
    
    float u = atan2(posBranch.z, posBranch.x);
    material = float4(kMatMapleBark, 0.0/*overriden for AO*/, u, progressAlong);

    return minDist;
}

float fBranchSDF(float3 posWS, float scale, float rand, out float4 material)
{
    float branchLen = 1.0 * scale;
    float branchRad = 0.03 * scale;

    return fBranchSDF(posWS, branchLen, branchRad, rand, material);
}

float fSmallBranchesSDF(float3 posWS, float branchDist, float branchProgress, out float4 material)
{
    float branchLen = clamp(branchDist * 2.0, 0.5, 2.0);
    
    //Shift until it looks good
    posWS += float3(0.53, 1.0, 0.53);
    
    float3 posBranches = posWS;
    float3 id = pMod3(posBranches, oz.xxx * 2.0);
    id.x += pModPolar(posBranches.yz, 3.0);
    float rand = hash13(id * 789.5336);
    posBranches.xz += sin((posBranches.y + rand) * 2.0 * kPI) * 0.05;
    pR(posBranches.xy, (rand - 0.5) * kPI * 0.25);
    float rad = 0.05 * (1.3 - ((posBranches.y/branchLen) + 0.5));
    float minDist = fCapsule(posBranches, rad, branchLen);
    
    posBranches = posWS + oz.xxx;
    id = oz.xxx * 235.68 + pMod3(posBranches, oz.xxx * 2.0);
    id.x += pModPolar(posBranches.yx, 3.0);
    rand = hash13(id * 789.5336);
    posBranches.xz += sin((posBranches.y + rand) * 2.0 * kPI) * 0.05;
    pR(posBranches.yz, (rand - 0.5) * kPI * 0.25);
    rad = 0.05 * (1.3 - ((posBranches.y/branchLen) + 0.5));
    minDist = min(minDist, fCapsule(posBranches, rad, branchLen));
    
    //Remove branches near the center of the tree
    minDist += saturate(0.7 - branchProgress);
    
    //Remove small branches away from main branches
    minDist = smax(minDist, branchDist - 3.0, 0.75);
    //Remove small branches past the end of the main branch
    minDist += saturate((branchProgress - 1.4)*3.0);
    
    material = float4(kMatMapleBark, 0.0/*overriden for AO*/, 0.001, branchProgress);
    return minDist;
}

float fCanopy(float3 posTreeSpace, float branchesDist, float4 branchMaterial, out float4 material)
{
    const float leafSize = 0.15;
    const float leafRep = 0.4;
    
    float3 leavesPos = posTreeSpace;
    float3 leafId = pMod3(leavesPos, oz.xxx * leafRep);
    float leafRand = hash13(leafId * 347.0468);
    leavesPos.xzy += (leafRand - 0.5) * oz.xxx * leafRep * 0.5;
    pR(leavesPos.xz, leafRand * kPI);
    float leavesDist = fMapleLeaf(leavesPos, leafSize, leafRand, false, material);
    
    leavesPos = posTreeSpace + oz.xxx * leafRep * 0.5;
    pR(leavesPos.xz, kGoldenRatio * kPI);
    leafId = pMod3(leavesPos, oz.xxx * leafRep);
    leafRand = hash13(leafId * 347.0468);
    pR(leavesPos.xz, leafRand * kPI);
    leavesPos.xzy += (leafRand - 0.5) * oz.xxx * leafRep * 0.2;
    leavesDist = min(leavesDist, fMapleLeaf(leavesPos, leafSize, leafRand, false, material));
    
    //Remove leaves that are too far from branches, and too close to the trunk, and to far past the main branches
    float branchStart = linearstep(0.6, 0.4, branchMaterial.w);
    float branchEnd = linearstep(1.3, 1.42, branchMaterial.w);
    leavesDist = max(leavesDist, branchesDist - (0.27 - branchEnd*0.17) + branchStart);
    
    return leavesDist;
}

float fFallenLeavesSet(float3 posTreeSpace, float iter, float groundY, out float4 material)
{
    float iterRand = frac(iter * kGoldenRatio);
    float repSize = 0.25 + iter * 0.2;
    float3 leafPos = posTreeSpace - iter * kWindVelocityWS * 4.0;
    leafPos.y = groundY - min(0.25, 0.75 - dot(leafPos.xz, leafPos.xz)*(0.01 - iter*0.002)*0.75);
    float2 leafId = pMod2(leafPos.xz, oz.xx * repSize);
    float rand = tree_hash12((leafId + oz.xx*iterRand*25.0) * 93.67);

    leafPos.xz += (rand - 0.5) * oz.xx * repSize * (0.5);
    leafPos.y += abs(rand-0.5)*2.0 * 0.75;
    
    pR(leafPos.yz, kPI * (0.2 + rand * 0.8));
    pR(leafPos.xy, kPI * 2.0 * rand);

    return fMapleLeaf(leafPos, 0.15, rand, true, /*out*/material);
}

float fFallenLeaves(float3 posTreeSpace, float groundY, out float4 material)
{
    float minDist = kMaxDist;
    
    if(groundY < 1.)
    {
        float4 fallenLeavesMaterial;
        float fallenLeavesDist = fFallenLeavesSet(posTreeSpace, 0.0, groundY - 0.3, /*out*/fallenLeavesMaterial);
        if(fallenLeavesDist < minDist)
        {
            minDist = fallenLeavesDist;
            material = fallenLeavesMaterial;
        }

        fallenLeavesDist = fFallenLeavesSet(posTreeSpace, 1.0, groundY - 0.2, /*out*/fallenLeavesMaterial);
        if(fallenLeavesDist < minDist)
        {
            minDist = fallenLeavesDist;
            material = fallenLeavesMaterial;
        }

        fallenLeavesDist = fFallenLeavesSet(posTreeSpace, 2.0, groundY, /*out*/fallenLeavesMaterial);
        if(fallenLeavesDist < minDist)
        {
            minDist = fallenLeavesDist;
            material = fallenLeavesMaterial;
        }  
   
    }
    else
    {
        minDist = min(minDist, groundY - 0.75);
    }
    
    return minDist;
}

float fTreeSDF(float3 posTreeSpace, float groundY, out float4 material)
{
    float minDist = kMaxDist;
    float treeBoundingSphereDist = fSphere(posTreeSpace - oz.yxy * 8.0, 9.0);
    
    // If we're far from the tree and falling leaves, bail
    if(treeBoundingSphereDist > 12.0)
    {
        return treeBoundingSphereDist - 10.0;
    }
    
    // Falling leaves
    {
        float3 leafPos = posTreeSpace - oz.yxy * 10.0;
        pR(leafPos.xy, -kPI * 0.25);//Rotate to match wind direction
        float leafId = pModPolar(leafPos.xz, 9.0);
        float leafRand = tree_hash11(leafId * 68.76);
        leafPos.x -= 4.0 + leafRand * 2.0;
        float fallDuration = 20.0;
        float fallTime = (s_time*1.75 + leafRand * fallDuration);
        float iter = floor(fallTime / fallDuration);
        fallTime = fallTime - iter * fallDuration;
        
        leafPos.y += 2.0 + fallTime;
        float xOff = sin(fallTime * 0.75 + iter * kPI) + cos(fallTime * 1.5 + iter * kPI) * 0.5;
        leafPos.x += xOff*1.0;
        
        if(length(leafPos) > 0.3)
        {
            minDist = length(leafPos) - 0.2;
        }
        else
        {
            pR(leafPos.yz, xOff * 0.5 * kPI);
            pR(leafPos.xy, fallTime * (leafRand + 1.0) + iter * kPI);

            float4 fallingLeavesMaterial;
            float fallingLeavesDist = fMapleLeaf(leafPos, 0.15, leafRand, true, /*out*/fallingLeavesMaterial);
            fallingLeavesMaterial.x = kMatFallingMapleLeaf;
            if(fallingLeavesDist < minDist)
            {
                minDist = fallingLeavesDist;
                material = fallingLeavesMaterial;
            }
        }
    }
    
    // If we're far from the tree, bail early
    if(treeBoundingSphereDist > 1.0)
    {
        return min(minDist, treeBoundingSphereDist);
    }
    
    float4 trunkMaterial;
    float3 trunkPos = posTreeSpace;
    
    float trunkDist = fBranchSDF(trunkPos, 10.0, 0.5, 0.0, trunkMaterial);
        
    if(trunkDist < minDist)
    {
        minDist = trunkDist;
        material = trunkMaterial;
    }
    
    float minBranchDist = kMaxDist;
    float4 minBranchMaterial;
    
    
    float winFlexOffset = dot(sin(posTreeSpace * 0.1), oz.xxx) * 2.0 * kPI;
    float windFlexAmount = min(8.0, trunkDist)/8.0;
    float3 windOffset = float3(kWindVelocityWS.x, 0.5, kWindVelocityWS.z) * 
        (sin(s_time * 4.0 + winFlexOffset)) * 
        0.05 * windFlexAmount;
        
    
    float4 branchMaterial;
    float3 branchPos;
    float branchDist, id, rand;
    
    branchPos = trunkPos;
    id = pModPolar(branchPos.xz, 6.0);
    rand = tree_hash11(id * 736.884);
    branchPos.y -= 4.0 + 1.0 * rand;
    pR(branchPos.xy, -kPI * (0.32 + rand * 0.1));
    
    branchDist = fBranchSDF(branchPos, 5.75, rand, branchMaterial);
    if(branchDist < minBranchDist)
    {
        minBranchDist = branchDist;
        minBranchMaterial = branchMaterial;
    }
    
    branchPos = trunkPos;
    pR(branchPos.xz, -kPI * 0.35);
    id = pModPolar(branchPos.xz, 5.0);
    rand = tree_hash11(id * 736.884);
    branchPos.y -= 7.5 + 1.0 * rand;
    pR(branchPos.xy, -kPI * (0.35 - rand * 0.05));
   
    branchDist = fBranchSDF(branchPos, 5.0, 0.0, branchMaterial);
    if(branchDist < minBranchDist)
    {
        minBranchDist = branchDist;
        minBranchMaterial = branchMaterial;
    }
    
    branchPos = trunkPos;
    pR(branchPos.xz, -kPI * 0.65);
    id = pModPolar(branchPos.xz, 3.0);
    rand = tree_hash11(id * 736.884);
    branchPos.y -= 9.5 + 0.5 * rand;
    pR(branchPos.xy, -kPI * (0.22 - 0.1 * rand));
    
    branchDist = fBranchSDF(branchPos, 4.0, 0.0, branchMaterial);
    if(branchDist < minBranchDist)
    {
        minBranchDist = branchDist;
        minBranchMaterial = branchMaterial;
    }
    
    if(minBranchDist < minDist)
    {
        minDist = minBranchDist;
        material = minBranchMaterial;
    }
    
    
    float4 smallBranchesMaterial;
    float smallBranchesDist = fSmallBranchesSDF(trunkPos + windOffset * 0.25, minBranchDist, minBranchMaterial.w, 
                                                /*out*/smallBranchesMaterial);
    if(smallBranchesDist < minDist)
    {
        minDist = smallBranchesDist;
        material = smallBranchesMaterial;
    }

    float4 leavesMaterial;
    float leavesDist = fCanopy(trunkPos + windOffset, smallBranchesDist, minBranchMaterial, 
                               /*out*/leavesMaterial);
    
    if(leavesDist < minDist)
    {
        minDist = leavesDist;
        material = leavesMaterial;
    }
    
    // Ambient occlusion is stronger at the center of the tree
    float3 posToCanopyCenter = float3(0.0, 10.0, 0.0) - posTreeSpace;
    material.y = min(1.0, dot(posToCanopyCenter, posToCanopyCenter) / 36.0);

    return minDist;
}

float fGrassBladeSet(float3 grassPosWS, float iter, float scale, float flattenAmount, inout float4 material)
{
    float iterRand = tree_hash11(iter * 967.367);
    float height = 0.45 * max(1.0, scale);
    float2 repSize = float2(0.15, 0.15) * scale;
    
    
    float windOffset = iterRand * 5.0 + cos(grassPosWS.z * 2.0)*1.0;
    float wind = sin(s_time * 2.5 + windOffset + 2.0 * grassPosWS.x) * (1.0 - flattenAmount);
    
    //Offset each set to prevet overlap
    grassPosWS.xz += repSize * kGoldenRatio * iter * oz.xx;
    //Rotate each set in a different direction
    pR(grassPosWS.xz, (0.25 + (iterRand - 0.5) * 0.5) * kPI);
    
    float2 id = pMod2(grassPosWS.xz, repSize);
    float rand = tree_hash12(id);
    
    float normalisedHeight = saturate(grassPosWS.y / height);
        
    //Rotate/bend each blade with the wind
    pR(grassPosWS.yz, (normalisedHeight * (0.05 + rand * 0.1) + wind * 0.025) * kPI);
    grassPosWS.xz += (hash22(id * 37.3468) - 0.5*oz.xx) * repSize * 0.75;
    
    //Rotate the blade arount Y to get a variety of normal directions
    pR(grassPosWS.xz, (rand - 0.5) * 2.0 * kPI);
    
    static const float kConeInvAngle = 0.485*PI;
    const float2 kRefConeSinCos = float2(sin(kConeInvAngle), cos(kConeInvAngle));
    float grassD = sdCone(grassPosWS - oz.yxy*height, float3(kRefConeSinCos, height));  
    grassD = max(grassD, abs(grassPosWS.x) - 0.005);
    
    grassD += flattenAmount * 0.2;
    
    float ambientVis = min(1.0, 1.7 * normalisedHeight)*(1.0-flattenAmount);
    material = float4(kMatGrass, normalisedHeight, min(material.z, ambientVis), rand);
    
    return grassD * 0.8;
}

float fGrass(float3 posWS, float groundY, float leavesDist, out float4 material)
{      
    float3 grassPosWS;
    
    grassPosWS = posWS;
    grassPosWS.y = groundY + max(0.0, length(posWS.xz - s_eyePositionWS.xz) - 15.0) / 60.0;
    
    // Early ouut if far from the ground
    if(grassPosWS.y > 1.0)
    {
        return grassPosWS.y - 0.2;
    }
    
    material.z = 1.0;
    
    float flattenAmount = linearstep(0.1, 0.0, leavesDist);
    float grassDist = kMaxDist;    
    
    grassDist = min(grassDist, fGrassBladeSet(grassPosWS, 1.0, 1.0, flattenAmount, /*out*/material));
    grassDist = min(grassDist, fGrassBladeSet(grassPosWS, 2.0, 2.0, flattenAmount, /*out*/material));
    
    material.z *= linearstep(0.0, 0.3, leavesDist);

    return grassDist;
}

// fbm implementation from https://www.shadertoy.com/view/WslcR2
float pseudo_random(float2 p)
{
    float x = dot(p, float2(4371.321, -9137.327));
    return 2.0 * frac(sin(x) * 17381.94472) - 1.0;
}

float noise_for_fmb(in float2 p)
{
    float2 id = floor(p);
    float2 f = frac(p);
    
    float2 u = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(pseudo_random(id + float2(0.0, 0.0)),
                   pseudo_random(id + float2(1.0, 0.0)), u.x),
               lerp(pseudo_random(id + float2(0.0, 1.0)),
                   pseudo_random(id + float2(1.0, 1.0)), u.x),
               u.y);
}

float noiseFbm(float2 p)
{
    float f = 0.0;
    float gat = 0.0;
    
    for (float octave = 0.; octave < 5.; ++octave)
    {
        float la = pow(2.0, octave);
        float ga = pow(0.5, octave + 1.);
        f += ga * noise_for_fmb(la * p);
        gat += ga;
    }
    
    f = f / gat;
    
    return f;
}

// float noiseFbm(float2 uv, sampler2D noiseSampler)
// {
//     float fbm = 0.0;
//     float noise;
//     
//     noise = textureLod(noiseSampler, uv * 1.0, 0.0).r;
//     fbm += noise * 1.0;
//     noise = textureLod(noiseSampler, uv * 1.5, 0.0).r;
//     fbm += noise * 0.55;
//     noise = textureLod(noiseSampler, uv * 3.0, 0.0).r;
//     fbm += noise * 0.35;
//     noise = textureLod(noiseSampler, uv * 4.5, 0.0).r;
//     fbm += noise * 0.25;    
//     return fbm;
// }

float fSDF(float3 posWS, uint filterId, out float4 material)
{    
    float mountainNoise = noiseFbm(posWS.xz * 0.0001 + oz.xx * 0.28);
    
    float minDist = kMaxDist;
    
    float groundY = fSphere(posWS - float3(-10.0, -500.0, -20.0), 500.0);
    
    float distantGroundDist = (mountainNoise - 0.25) * 30.0 + fSphere(posWS - float3(100.0, -4995.0, 500.0), 5000.0);
    
    float allMountainsDist = kMaxDist;
    // Tree
    float4 treeMaterial;
    float treeDist = fTreeSDF(posWS - kTreePosWS, groundY, /*out*/ treeMaterial);
    
    if(treeDist < minDist)
    {
        minDist = treeDist;
        material = treeMaterial;
    }
    
    return minDist;
}

float fSDF(float3 p)
{
    float4 mat;
    return fSDF(p, kRenderFilter, mat);
}

float funDist(float3 p)
{
    static const float scale = 0.1;
    p *= 1./scale;
    p -= float3(0.,-6.,9.);
    return fSDF(p) * scale;
}

#endif
