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
uniform float worldMode;
uniform vec2 maskTargetSize;
uniform vec2 maskViewSize;
uniform vec2 maskViewPosition;
uniform float maskViewRotation;
uniform vec2 regionPosition;

vec2 rotate2D(vec2 v, float a)
{
    float s = sin(a);
    float c = cos(a);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

void main()
{
    vec4 pixel = texture2D(texture, sf_TexCoord0.xy);
    float alpha = clamp(pixel.a * sf_FrontColor.a, 0.0, 1.0);
    if (alpha <= 0.0)
        discard;

    vec2 totalPixelSize = mapSize * lightBlockSize;
    vec2 localPixelPosition;
    if (worldMode > 0.5)
    {
        vec2 targetScale = maskTargetSize / maskViewSize;
        vec2 pixelPositionBL = gl_FragCoord.xy / targetScale;
        vec2 pixelPositionTL = vec2(
            pixelPositionBL.x,
            maskViewSize.y - pixelPositionBL.y
        );
        float radians = maskViewRotation * 0.017453292519943295;
        vec2 viewCentre = maskViewPosition + maskViewSize * 0.5;
        vec2 worldPosition = viewCentre + rotate2D(
            pixelPositionTL - maskViewSize * 0.5,
            radians
        );
        localPixelPosition = worldPosition - regionPosition;
        if (
            localPixelPosition.x < 0.0 ||
            localPixelPosition.y < 0.0 ||
            localPixelPosition.x >= totalPixelSize.x ||
            localPixelPosition.y >= totalPixelSize.y
        )
        {
            discard;
        }
    }
    else
    {
        localPixelPosition = gl_FragCoord.xy;
        localPixelPosition.y = totalPixelSize.y - localPixelPosition.y;
    }

    vec2 cellUV = localPixelPosition / totalPixelSize;
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
