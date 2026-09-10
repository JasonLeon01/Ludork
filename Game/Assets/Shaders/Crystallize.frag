#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif

uniform sampler2D texture;
uniform float time;
uniform vec2 textureSize;
uniform vec4 textureRect;

void main()
{
    vec2 uv = sf_TexCoord0.xy;
    vec4 texColor = texture2D(texture, uv);
    vec2 pixelPosition = uv * textureSize;
    vec2 localUv = clamp(
        (pixelPosition - textureRect.xy) / max(textureRect.zw, vec2(1.0)),
        0.0,
        1.0
    );

    vec2 facetCell = floor(localUv * vec2(9.0, 11.0));
    float facetNoise = fract(sin(dot(facetCell, vec2(12.9898, 78.233))) * 43758.5453);
    float facetPlane = 0.72 + facetNoise * 0.38;
    float diagonal = localUv.x + localUv.y * 0.62;
    float sweepPosition = mod(time * 0.42, 1.85) - 0.18;
    float sweep = 1.0 - smoothstep(0.0, 0.095, abs(diagonal - sweepPosition));
    float glint = 0.5 + 0.5 * sin(time * 2.8 + facetNoise * 6.28318);
    float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
    vec3 crystalBase = mix(
        vec3(0.20, 0.55, 0.78),
        vec3(0.72, 0.56, 0.96),
        facetNoise
    );
    vec3 facetedColor = mix(texColor.rgb, crystalBase * (0.58 + luminance * 0.72), 0.68);
    facetedColor *= facetPlane;
    facetedColor += vec3(0.58, 0.88, 1.0) * sweep * (0.42 + glint * 0.28);

    gl_FragColor = vec4(clamp(facetedColor, 0.0, 1.0), texColor.a) * sf_FrontColor;
}
