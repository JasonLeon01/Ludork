uniform sampler2D screenTex;
uniform sampler2D fogTex;
uniform vec2 texSize;
uniform vec2 fogScroll;
uniform float power;
uniform float distort;
uniform float time;

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
    vec2 uv = gl_TexCoord[0].xy;
    vec3 src = texture2D(screenTex, clamp(uv, 0.0, 1.0)).rgb;
    float p = clamp(power, 0.0, 1.0);
    if (p <= 0.001)
    {
        gl_FragColor = vec4(src, 1.0);
        return;
    }

    float distortAmt = clamp(distort, 0.0, 1.0) * 0.22;
    vec2 fogUV = uv * 2.0 + fogScroll;
    fogUV = flowWarp(fogUV, distortAmt, time, 0.0);
    vec2 fogUV2 = fogUV * 1.55 + fogScroll * 0.6;
    fogUV2 = flowWarp(fogUV2, distortAmt * 0.75, time, 2.17);
    float d1 = texture2D(fogTex, fract(fogUV)).a;
    float d2 = texture2D(fogTex, fract(fogUV2)).a * 0.45;
    float density = clamp(smoothstep(0.12, 0.58, d1 + d2), 0.0, 1.0) * p;
    vec3 fogColor = vec3(0.92, 0.94, 0.97);
    vec3 outCol = mix(src, fogColor, density);
    gl_FragColor = vec4(clamp(outCol, 0.0, 1.0), 1.0);
}
