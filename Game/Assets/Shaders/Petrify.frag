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
    float layeredLight = floor(luminance * 5.0 + 0.5) / 5.0;
    vec2 grainCell = floor(localUv * vec2(31.0, 37.0));
    float grain = fract(
        sin(dot(grainCell, vec2(17.171, 43.117)) + time * 0.48) * 15731.743
    );
    float strata = 0.5 + 0.5 * sin(localUv.y * 31.0 + localUv.x * 7.0);
    vec3 darkStone = vec3(0.25, 0.25, 0.23);
    vec3 lightStone = vec3(0.69, 0.66, 0.57);
    vec3 stoneColor = mix(darkStone, lightStone, layeredLight);
    stoneColor *= 0.87 + grain * 0.17;
    stoneColor += vec3(0.055, 0.047, 0.034) * strata;
    stoneColor = mix(stoneColor, texColor.rgb * vec3(0.72, 0.69, 0.61), 0.16);

    gl_FragColor = vec4(clamp(stoneColor, 0.0, 1.0), texColor.a) * sf_FrontColor;
}
