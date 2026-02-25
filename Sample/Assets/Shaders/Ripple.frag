uniform sampler2D screenTex;
uniform vec2 texSize;
uniform vec2 center;
uniform float radius;
uniform float thickness;
uniform float strength;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec3 src = texture2D(screenTex, clamp(uv, 0.0, 1.0)).rgb;

    if (strength <= 0.0 || thickness <= 0.0 || radius <= 0.0) {
        gl_FragColor = vec4(src, 1.0);
        return;
    }

    vec2 toCenterUV = uv - center;
    float lenSq = dot(toCenterUV, toCenterUV);
    if (lenSq <= 1e-12) {
        gl_FragColor = vec4(src, 1.0);
        return;
    }

    vec2 toCenterPX = toCenterUV * texSize;
    float distPX = length(toCenterPX);

    float x = (distPX - radius) / thickness;
    float band = clamp(1.0 - abs(x), 0.0, 1.0);
    band = smoothstep(0.0, 1.0, band);
    band = band * band;

    vec2 dir = normalize(toCenterUV);
    vec2 pixelToUV = 1.0 / texSize;
    vec2 offsetUV = dir * (band * strength) * pixelToUV;

    vec3 col = texture2D(screenTex, clamp(uv + offsetUV, 0.0, 1.0)).rgb;
    gl_FragColor = vec4(col, 1.0);
}
