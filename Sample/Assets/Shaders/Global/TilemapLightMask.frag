#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif

uniform sampler2D texture;
uniform sampler2D lightBlockTex;
uniform sampler2D reflectionStrengthTex;
uniform sampler2D ignoreLightingTex;
uniform vec2 lightBlockSize;
uniform vec2 mapSize;
uniform float transmissionMode;

void main()
{
    vec4 pixel = texture2D(texture, sf_TexCoord0.xy);
    float alpha = clamp(pixel.a * sf_FrontColor.a, 0.0, 1.0);
    if (alpha <= 0.0)
        discard;

    vec2 totalPixelSize = mapSize * lightBlockSize;
    vec2 worldPos = gl_FragCoord.xy;
    worldPos.y = totalPixelSize.y - worldPos.y;

    vec2 cellUV = worldPos / totalPixelSize;
    float blockVal = clamp(texture2D(lightBlockTex, cellUV).r, 0.0, 1.0);
    float reflectionStrengthVal = clamp(
        texture2D(reflectionStrengthTex, cellUV).r,
        0.0,
        1.0
    );
    float ignoreLightingVal = clamp(
        texture2D(ignoreLightingTex, cellUV).r,
        0.0,
        1.0
    );
    float maskValue = blockVal;
    if (transmissionMode > 0.5)
        maskValue = 1.0 - alpha * blockVal;
    gl_FragColor = vec4(
        maskValue,
        reflectionStrengthVal,
        ignoreLightingVal,
        alpha
    );
}
