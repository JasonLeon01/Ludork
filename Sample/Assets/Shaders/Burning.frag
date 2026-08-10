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

    float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
    float risingWave = 0.5 + 0.5 * sin(
        localUv.x * 20.0 + localUv.y * 9.0 - time * 5.4
    );
    float heatWave = 0.5 + 0.5 * sin(
        localUv.x * 8.0 - localUv.y * 23.0 + time * 3.7
    );
    float flicker = 0.5 + 0.5 * sin(time * 9.0 + localUv.x * 13.0);
    float heat = clamp(
        (1.0 - localUv.y) * 0.32 + risingWave * 0.38 + heatWave * 0.18 + flicker * 0.12,
        0.0,
        1.0
    );
    vec3 emberColor = mix(vec3(0.72, 0.08, 0.015), vec3(1.0, 0.72, 0.08), heat);
    vec3 burningColor = mix(texColor.rgb, emberColor * (0.68 + luminance * 0.62), 0.68);
    burningColor += vec3(0.24, 0.065, 0.0) * flicker * heat;

    gl_FragColor = vec4(clamp(burningColor, 0.0, 1.0), texColor.a) * sf_FrontColor;
}
