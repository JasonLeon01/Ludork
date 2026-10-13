#ifdef GL_ES
precision highp float;
#endif

uniform vec2 screenSize;
uniform vec2 viewPos;
uniform vec2 viewSinCos;
uniform vec2 pixelSize;
uniform vec2 regionOrigin;
uniform vec2 regionSize;
uniform vec4 fogColour;
uniform float density;
uniform float noiseScale;
uniform vec2 velocity;
uniform float turbulence;
uniform float seed;
uniform float time;
uniform float lit;
uniform sampler2D ambientLight;
uniform sampler2D directLight;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7)) + seed * 17.17) * 43758.5453);
}

float noise(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(cell), hash(cell + vec2(1.0, 0.0)), f.x),
               mix(hash(cell + vec2(0.0, 1.0)), hash(cell + vec2(1.0, 1.0)), f.x), f.y);
}

float field(vec2 p) {
    float result = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 4; ++octave) {
        result += noise(p) * amplitude;
        p = mat2(1.6, 1.2, -1.2, 1.6) * p + vec2(23.7, 15.3);
        amplitude *= 0.5;
    }
    return result;
}

void main() {
    vec2 uv = gl_FragCoord.xy / pixelSize;
    vec2 offset = (vec2(uv.x, 1.0 - uv.y) - 0.5) * screenSize;
    vec2 world = viewPos + screenSize * 0.5 + vec2(
        offset.x * viewSinCos.y - offset.y * viewSinCos.x,
        offset.x * viewSinCos.x + offset.y * viewSinCos.y);
    vec2 local = world - regionOrigin;
    if (local.x < 0.0 || local.y < 0.0 || local.x >= regionSize.x || local.y >= regionSize.y)
        discard;
    vec2 p = (world - velocity * time) / noiseScale;
    vec2 warp = vec2(field(p + vec2(3.7, 9.2)), field(p + vec2(21.4, 7.8)));
    float value = field(p + (warp - 0.5) * turbulence * 3.0) / 0.9375;
    float coverage = smoothstep(0.25, 0.75, value);
    float alpha = (1.0 - exp(-2.0 * density * coverage)) * fogColour.a;
    vec3 light = mix(vec3(1.0), clamp(texture2D(ambientLight, uv).rgb + texture2D(directLight, uv).rgb, 0.0, 1.0), lit);
    gl_FragColor = vec4(fogColour.rgb * light, alpha);
}
