/*
Copyright 2013 Inigo Quilez @iq
License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
Link: https://www.shadertoy.com/view/MsfGRr
Archive Link: https://web.archive.org/web/20191113091856/https://www.shadertoy.com/view/MsfGRr
*/

/******************************************************************************
 This work is a derivative of work by Inigo Quilez used under CC BY-NC-SA 3.0.
 This work is licensed also under CC BY-NC-SA 3.0 by NVIDIA CORPORATION.
 ******************************************************************************/

#ifndef julia_glsl
#define julia_glsl

//--------------------------------------------------------------------------------
// quaternion manipulation
//--------------------------------------------------------------------------------

float lengthSquared(float4 a){
    return dot(a,a);
}

float4 qSquare( float4 a )
{
    return float4( a.x*a.x - dot(a.yzw,a.yzw), 2.0*a.x*(a.yzw) );
}

float4 qCube( float4 a )
{
    return a * ( 4.0*a.x*a.x - dot(a,a)*float4(3.0,1.0,1.0,1.0) );
}

float3 julia_map( float3 p, float4 c )
{
    float4 z = float4( p, 0.2 );
    
    float m2 = 0.0;
    float2  t = float2( 1e10 );

    float dz2 = 1.0;
    for( int i=0; i<10; i++ ) 
    {
        // |dz|² = |3z²|²
        dz2 *= 9.0*lengthSquared(qSquare(z));
        
        // z = z^3 + c		
        z = qCube( z ) + c;
        
        // stop under divergence		
        m2 = dot(z, z);		
        if( m2>10000.0 ) break;				 

        // orbit trapping ( |z|² and z_x  )
        t = min( t, float2( m2, abs(z.x)) );

    }

    // distance estimator: d(z) = 0.5·log|z|·|z|/|dz|   (see http://iquilezles.org/www/articles/distancefractals/distancefractals.htm)
    float d = 0.25 * log(m2) * sqrt(m2/dz2 );

    return float3( d, t );
}

float funDist(float3 p)
{
    const float t = 10.;
    const float4 c = float4(-0.1,0.6,0.9,-0.3) + 0.1*sin( float4(3.0,0.0,1.0,2.0) + 0.5*float4(1.0,1.3,1.7,2.1)*t);
    const float scale = 0.8;
    p *= 1. / scale;
    return julia_map(p, c).x * scale;
}

#endif
