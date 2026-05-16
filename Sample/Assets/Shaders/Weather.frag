uniform sampler2D screenTex;
uniform vec2 texSize;
uniform float time;
uniform float weatherType;
uniform float power;
uniform float maxScale;

float hash21(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float rainStreak(vec2 uv, float t, float density, vec2 scroll)
{
    vec2 p = uv + scroll;
    vec2 grid = vec2(80.0, 30.0) * (0.35 + density * 0.85);
    vec2 cell = floor(p * grid);
    vec2 f = fract(p * grid);
    float n = hash21(cell);
    if (n > density)
    {
        return 0.0;
    }
    float id = hash21(cell + 2.17);
    float phase = fract(t * (0.35 + id * 0.55) + id);
    float along = fract(f.y - phase);
    if (along > 0.6)
    {
        return 0.0;
    }
    float slant = 0.18;
    float across = abs(f.x - 0.5 + along * slant + (id - 0.5) * 0.2);
    return smoothstep(0.2, 0.0, across) * smoothstep(0.6, 0.0, along);
}

float snowFlake(vec2 uv, float t, float density, vec2 scroll)
{
    vec2 p = uv + scroll;
    vec2 grid = vec2(50.0, 50.0) * (0.4 + density * 0.75);
    vec2 cell = floor(p * grid);
    vec2 f = fract(p * grid) - 0.5;
    float n = hash21(cell);
    if (n > density * 0.9)
    {
        return 0.0;
    }
    float dist = length(f);
    float flake = smoothstep(0.2, 0.0, dist);
    float twinkle = 0.7 + 0.3 * sin(t * 2.5 + n * 6.28318);
    return flake * twinkle;
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec3 src = texture2D(screenTex, clamp(uv, 0.0, 1.0)).rgb;
    float p = clamp(power, 0.0, 1.0);
    float m = clamp(maxScale, 0.1, 2.0);
    int wType = int(weatherType + 0.5);

    if (wType == 0 || p <= 0.001)
    {
        gl_FragColor = vec4(src, 1.0);
        return;
    }

    float density = clamp(0.12 + p * m * 0.6, 0.08, 0.92);

    if (wType == 1 || wType == 2)
    {
        float t = time;
        float rain = 0.0;
        rain += rainStreak(uv, t, density, vec2(-t * 0.07, t * 0.5));
        rain += rainStreak(uv, t * 1.08, density * 0.92, vec2(-t * 0.1 + 0.37, t * 0.58)) * 0.65;
        rain += rainStreak(uv, t * 0.92, density * 0.85, vec2(-t * 0.05 + 0.71, t * 0.42)) * 0.45;
        rain = clamp(rain, 0.0, 1.0);
        vec3 rainCol = vec3(0.78, 0.86, 1.0);
        src += rainCol * rain * p * 0.42;
        if (wType == 2)
        {
            src *= mix(vec3(1.0), vec3(0.6, 0.66, 0.78), p * 0.32);
        }
    }
    else if (wType == 3)
    {
        float t = time;
        float snow = 0.0;
        snow += snowFlake(uv, t, density, vec2(sin(t * 0.25) * 0.015, t * 0.022));
        snow += snowFlake(uv, t * 0.88, density * 0.85, vec2(-t * 0.01, t * 0.018)) * 0.6;
        snow = clamp(snow, 0.0, 1.0);
        src += vec3(0.95, 0.97, 1.0) * snow * p * 0.38;
    }

    gl_FragColor = vec4(clamp(src, 0.0, 1.0), 1.0);
}
