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

    float phase = (localUv.x + localUv.y * 0.35) * 6.28318 + time * 2.0;
    vec3 rainbow = vec3(
        sin(phase) * 0.5 + 0.5,
        sin(phase + 2.094) * 0.5 + 0.5,
        sin(phase + 4.189) * 0.5 + 0.5
    );
    vec3 finalColor = mix(texColor.rgb, rainbow, 0.5);

    gl_FragColor = vec4(finalColor, texColor.a) * sf_FrontColor;
}
