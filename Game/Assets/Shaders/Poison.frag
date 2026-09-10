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
    float flowA = 0.5 + 0.5 * sin(localUv.y * 18.0 + localUv.x * 7.0 - time * 3.0);
    float flowB = 0.5 + 0.5 * sin(localUv.x * 25.0 - localUv.y * 11.0 + time * 2.15);
    float bubble = smoothstep(0.78, 1.0, flowA * flowB);
    float poisonMix = clamp(flowA * 0.55 + bubble * 0.45, 0.0, 1.0);
    vec3 poisonTint = mix(vec3(0.48, 0.12, 0.61), vec3(0.29, 0.91, 0.19), poisonMix);
    vec3 poisonColor = mix(texColor.rgb, poisonTint * (0.58 + luminance * 0.68), 0.62);
    poisonColor += vec3(0.20, 0.36, 0.08) * bubble;

    gl_FragColor = vec4(clamp(poisonColor, 0.0, 1.0), texColor.a) * sf_FrontColor;
}
