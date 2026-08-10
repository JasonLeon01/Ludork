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
    float pulse = 0.5 + 0.5 * sin(time * 2.35 + localUv.y * 2.6);
    float scanline = 0.5 + 0.5 * sin(localUv.y * 58.0 - time * 5.2);
    vec3 ghostTint = mix(vec3(0.12, 0.57, 0.66), vec3(0.58, 1.0, 0.94), luminance);
    vec3 ghostColor = mix(texColor.rgb, ghostTint, 0.72);
    ghostColor += vec3(0.08, 0.22, 0.22) * (pulse * 0.55 + scanline * 0.18);
    float ghostAlpha = texColor.a * (0.48 + pulse * 0.24 + scanline * 0.08);

    gl_FragColor = vec4(clamp(ghostColor, 0.0, 1.0), ghostAlpha) * sf_FrontColor;
}
