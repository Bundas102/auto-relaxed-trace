/*
Copyright 2017 @dr2
License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
Link: https://www.shadertoy.com/view/ll2BzR
*/

/******************************************************************************
 This work is a derivative of work by dr2 used under CC BY-NC-SA 3.0.
 This work is licensed also under CC BY-NC-SA 3.0 by NVIDIA CORPORATION.
 ******************************************************************************/

#ifndef boat_glsl
#define boat_glsl

float mod(float x, float y)
{
    return x - y * floor(x / y);
}



float PrBoxDf (float3 p, float3 b)
{
  float3 d;
  d = abs (p) - b;
  return min (max (d.x, max (d.y, d.z)), 0.) + length (max (d, 0.));
}

float PrBox2Df (float2 p, float2 b)
{
  float2 d;
  d = abs (p) - b;
  return min (max (d.x, d.y), 0.) + length (max (d, 0.));
}

float PrCylDf (float3 p, float r, float h)
{
  return max (length (p.xy) - r, abs (p.z) - h);
}

float PrCapsDf (float3 p, float r, float h)
{
  return length (p - float3 (0., 0., h * clamp (p.z / h, -1., 1.))) - r;
}

float PrEECapsDf (float3 p, float3 v1, float3 v2, float r)
{
  float3 s, t;
  s = p - v1;
  t = v2 - v1;
  return length (s - clamp (dot (s, t) / dot (t, t), 0., 1.) * t) - r;
}

float PrEllips2Df (float3 p, float2 r)
{
  return (length (p / r.xxy) - 1.) * min (r.x, r.y);
}

float2 Rot2D (float2 q, float a)
{
  return q * cos (a) + q.yx * sin (a) * float2 (-1., 1.);
}

float2 Rot2Cs (float2 q, float2 cs)
{
  return float2 (dot (q, float2 (cs.x, - cs.y)), dot (q.yx, cs));
}

static float3 shipConf, qHit, bDeck;
static float shipRot;
static float szFac = 0.6;
static float dstFar = 100.;
static int idObj;
static const int idHull = 1, idRud = 2, idStruc = 3, idMast = 4, idSparT = 5, idSparL = 6, idSailT = 7,
   idSailA = 8, idSailF = 9, idFlag = 10, idRig = 11, idShell = 21, idArm = 22, idHing = 23,
   idMir = 24, idLeg = 25;

#define DMINQ(id) if (d < dMin) { dMin = d;  idObj = id;  qHit = q; }

float ShipDf (float3 p)
{
  float3 q, qq, w;
  float dMin, d, fy, fz, gz, s, rRig, rSpar, yLim, zLim;
  rRig = 0.02;
  rSpar = 0.05;
  p.yz = Rot2D (p.yz, -0.4 * shipConf.z);
  p.xy = Rot2D (p.xy, 6. * shipConf.y);
  p.y -= shipConf.x + 4.1 * szFac;
  p /= szFac;
  dMin = dstFar / szFac;
  fy = 1. - 0.07 * p.y;
  fz = 1. - 0.14 * step (1., abs (p.z));
  zLim = abs (p.z) - 4.5;
  q = p;
  d = zLim;
  q.z = mod (q.z + 1.4, 2.8) - 1.2;
  d = max (d, PrCapsDf ((q - float3 (0., 3.7 * (fz - 1.), 0.)).xzy, 0.1 * fy, 3.7 * fz));
  DMINQ (idMast);
  q = p;
  yLim = abs (q.y - 0.2 * fz) - 3. * fz;
  qq = q;
  qq.y = mod (qq.y - 3.3 * (fz - 1.), 2. * fz) - fz;
  qq.z = mod (qq.z + 1.4, 2.8) - 1.4 + 0.1 * fz;
  d = max (max (min (d, PrCylDf (float3 (qq - float3 (0., 0.05 * fy * fz, 0.1 * fz - 0.23)).xzy,
     0.15 * fy, 0.11 * fy * fz)), yLim), zLim);
  DMINQ (idMast);
  d = max (max (PrCapsDf (qq.yzx, 0.05, 1.23 * fy * fz), yLim), zLim);
  DMINQ (idSparT);
  q = p;
  d = min (d, min (PrEECapsDf (q, float3 (0., -3.5, 4.3), float3 (0., -2.6, 6.7), rSpar),
     PrEECapsDf (q, float3 (0., -4., 4.1), float3 (0., -2.9, 6.), rSpar)));
  d = min (d, min (PrEECapsDf (q, float3 (0., -1.2, -3.), float3 (0., -0.5, -4.5), rSpar),
     PrEECapsDf (q, float3 (0., -2.7, -3.), float3 (0., -2.7, -4.5), rSpar)));
  DMINQ (idSparL);
  q = p;
  qq = q;
  qq.y = mod (qq.y - 3.1 * (fz - 1.), 2. * fz) - fz;
  qq.z = mod (qq.z + 1.4, 2.8) - 1.4 + 0.2 * (fz - abs (qq.y)) * (fz - abs (qq.y)) - 0.1 * fz;
  d = max (max (max (PrBoxDf (qq, float3 ((1.2 - 0.07 * q.y) * fz, fz, 0.01)),
     min (qq.y, 1.5 * fy * fz - length (float2 (qq.x, qq.y + 0.9 * fy * fz)))),
     abs (q.y - 3. * (fz - 1.)) - 2.95 * fz), - PrBox2Df (qq.yz, float2 (0.01 * fz)));
  d = max (d, zLim);
  DMINQ (idSailT);
  q = p;
  q.z -= -3.8;  q.y -= -1.75 - 0.2 * q.z;
  d = PrBoxDf (q, float3 (0.01, 0.9 - 0.2 * q.z, 0.6));
  DMINQ (idSailA);
  q = p;
  q.yz -= float2 (-1., 4.5);
  w = float3 (1., q.yz);
  d = max (max (max (abs (q.x) - 0.01, - dot (w, float3 (2.3, 1., -0.35))),
     - dot (w, float3 (0.68, -0.74, -1.))), - dot (w, float3 (0.41, 0.4, 1.)));
  DMINQ (idSailF);
  q = p;
  d = zLim;  
  gz = (q.z - 0.5) / 5. + 0.3;
  gz *= gz;
  gz = 1.05 * (1. - 0.45 * gz * gz);
  q.x = abs (q.x);
  q.z = mod (q.z + 1.4, 2.8) - 1.4;
  d = max (d, min (PrEECapsDf (q, float3 (1.05 * gz, -3.25, -0.5), float3 (1.4 * fz, -2.95, -0.05), 0.7 * rRig),
     PrEECapsDf (float3 (q.xy, abs (q.z + 0.2) - 0.01 * (0.3 - 2. * q.y)), float3 (gz, -3.2, 0.),
     float3 (0.05, -0.9 + 2. * (fz - 1.), 0.), rRig)));
  q = p;
  d = min (d, PrEECapsDf (q, float3 (0., -3., -4.45), float3 (0., -2.7, -4.5), 0.8 * rRig));
  d = min (min (d, min (PrEECapsDf (q, float3 (0., 2.45, 2.65), float3 (0., -2.7, 6.5), rRig),
     PrEECapsDf (q, float3 (0., 2.5, 2.65), float3 (0., -3.2, 4.9), rRig))),
     PrEECapsDf (q, float3 (0., 2.6, -3.), float3 (0., -0.5, -4.5), rRig));
  q.x = abs (q.x);
  d = min (d, PrEECapsDf (q, float3 (0.65, -3.5, 3.5), float3 (0.05, -2.7, 6.4), rRig));
  s = step (1.8, q.y) - step (q.y, -0.2);
  d = min (min (d, min (PrEECapsDf (q, float3 (0.95, 0.4, 2.7) + float3 (-0.1, 1.7, 0.) * s,
     float3 (0.05, 1.1, -0.15) + float3 (0., 2., 0.) * s, rRig),
     PrEECapsDf (q, float3 (1.05, 1., -0.1) + float3 (-0.1, 2., 0.) * s,
     float3 (0.05, 0.5, -2.95) + float3 (0., 1.7, 0.) * s, rRig))),
     PrEECapsDf (q, float3 (0.95, 0.4, -2.9) + float3 (-0.1, 1.7, 0.) * s,
     float3 (0.05, 0.9, -0.25) + float3 (0., 2., 0.) * s, rRig));
  DMINQ (idRig);
  q = p;
  q.yz -= float2 (3.4, 0.18);
  d = PrBoxDf (q, float3 (0.01, 0.2, 0.3));
  DMINQ (idFlag);
  q = p;
  d = zLim;
  q.z = mod (q.z + 1.4, 2.8) - 1.4;
  q.yz -= float2 (-3.4, -0.4);
  d = max (d, PrBoxDf (q, float3 (0.3, 0.1, 0.5)));
  DMINQ (idStruc);
  q = p;
  q.x = abs (q.x);
  q.yz -= float2 (-3.8, 0.5);
  fz = q.z / 5. + 0.3;
  fz *= fz;
  fy = 1. - smoothstep (-1.3, -0.1, q.y);
  gz = smoothstep (2., 5., q.z);
  bDeck = float3 ((1. - 0.45 * fz * fz) * (1.1 - 0.5 * fy * fy) *
     (1. - 0.5 * smoothstep (-5., -2., q.y) * smoothstep (2., 5., q.z)),
     0.78 - 0.8 * gz * gz - 0.2 * (1. - smoothstep (-5.2, -4., q.z)), 5. * (1. + 0. * 0.02 * q.y));
  d = min (PrBoxDf (float3 (q.x, q.y + bDeck.y - 0.6, q.z), bDeck),
     max (PrBoxDf (q - float3 (0., 0.72, -4.6), float3 (bDeck.x, 0.12, 0.4)),
     - PrBox2Df (float2 (abs (q.x) - 0.4, q.y - 0.65), float2 (0.2, 0.08))));
  d = max (d, - PrBoxDf (float3 (q.x, q.y - 0.58 - 0.1 * fz, q.z), float3 (bDeck.x - 0.07, 0.3, bDeck.z - 0.1)));
  q = p;
  d = max (d, - max (PrBox2Df (float2 (q.y + 3.35, mod (q.z + 0.25, 0.5) - 0.25), float2 (0.08, 0.1)),
     abs (q.z + 0.5) - 3.75));
  DMINQ (idHull);
  q = p;
  d = PrBoxDf (q + float3 (0., 4.4, 4.05), float3 (0.03, 0.35, 0.5));
  DMINQ (idRud);
  return 0.7 * dMin * szFac;
}

float funDist(float3 p){
    p += float3(0.,0.5,0.1);
    const float scale = 0.25;
    p *= (1.0 / scale);
    //p.z -= 3.0;
    return ShipDf(p) * scale;
}
#endif
