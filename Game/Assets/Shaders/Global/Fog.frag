#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_TexCoord0;
#endif

uniform sampler2D screenTex;
uniform sampler2D fogTex;
uniform vec2 fogScroll;
uniform float power;
uniform float distort;
uniform float time;
uniform float worldMode;
uniform vec2 worldOrigin;
uniform vec2 worldAxisX;
uniform vec2 worldAxisY;
uniform vec2 fogTextureSize;
uniform vec2 clipMin;
uniform vec2 clipMax;

float hash21(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float smoothNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

vec2 flowWarp(vec2 uv, float strength, float t, float phase)
{
    if (strength <= 0.0001)
    {
        return uv;
    }
    vec2 p0 = uv * 1.25 + vec2(t * 0.05 + phase, t * 0.035);
    vec2 p1 = uv * 2.4 + vec2(-t * 0.03 + phase * 1.7, t * 0.04);
    float n0x = smoothNoise(p0) * 2.0 - 1.0;
    float n0y = smoothNoise(p0 + vec2(19.2, 7.5)) * 2.0 - 1.0;
    float n1x = smoothNoise(p1) * 2.0 - 1.0;
    float n1y = smoothNoise(p1 + vec2(31.0, 11.4)) * 2.0 - 1.0;
    vec2 offset = vec2(n0x, n0y) * 0.7 + vec2(n1x, n1y) * 0.3;
    return uv + offset * strength;
}

void main()
{
    vec2 uv = sf_TexCoord0.xy;
    vec3 src = texture2D(screenTex, clamp(uv, 0.0, 1.0)).rgb;
    vec2 worldUV = vec2(uv.x, 1.0 - uv.y);
    vec2 worldPosition = worldOrigin + worldUV.x * worldAxisX + worldUV.y * worldAxisY;
    if (worldMode > 0.5 &&
        (worldPosition.x < clipMin.x || worldPosition.y < clipMin.y ||
         worldPosition.x > clipMax.x || worldPosition.y > clipMax.y))
    {
        gl_FragColor = vec4(src, 1.0);
        return;
    }
    float p = clamp(power, 0.0, 1.0);
    if (p <= 0.001)
    {
        gl_FragColor = vec4(src, 1.0);
        return;
    }

    float distortAmt = clamp(distort, 0.0, 1.0) * 0.22;
    vec2 fogUV = worldMode > 0.5
        ? worldPosition / max(fogTextureSize, vec2(1.0)) + fogScroll
        : uv * 2.0 + fogScroll;
    fogUV = flowWarp(fogUV, distortAmt, time, 0.0);
    vec2 fogUV2 = fogUV * 1.55 + fogScroll * 0.6;
    fogUV2 = flowWarp(fogUV2, distortAmt * 0.75, time, 2.17);
    vec4 fog1 = texture2D(fogTex, fract(fogUV));
    vec4 fog2 = texture2D(fogTex, fract(fogUV2));
    float d1 = fog1.a;
    float d2 = fog2.a * 0.45;
    float densityWeight = d1 + d2;
    float density = clamp(smoothstep(0.12, 0.58, densityWeight), 0.0, 1.0) * p;
    vec3 fogColor = (fog1.rgb * d1 + fog2.rgb * d2) /
                    max(densityWeight, 0.0001);
    vec3 outCol = mix(src, fogColor, density);
    gl_FragColor = vec4(clamp(outCol, 0.0, 1.0), 1.0);
}
