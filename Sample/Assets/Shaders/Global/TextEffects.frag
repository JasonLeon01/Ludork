#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif

uniform sampler2D texture;
uniform sampler2D outlineTexture;
uniform sampler2D curveTexture;
uniform vec2 textureSize;
uniform vec2 contentMinimum;
uniform vec2 contentMaximum;
uniform bool gradientEnabled;
uniform int gradientDirection;
uniform bool glowEnabled;
uniform vec4 glowColor;
uniform float glowRadius;
uniform float glowIntensity;

vec4 straightColor(vec4 sampleColor)
{
    if (sampleColor.a <= 0.0001)
        return vec4(0.0);
    return vec4(sampleColor.rgb / sampleColor.a, sampleColor.a);
}

float combinedAlpha(vec2 uv)
{
    float fillAlpha = texture2D(texture, uv).a;
    float outlineAlpha = texture2D(outlineTexture, uv).a;
    return max(fillAlpha, outlineAlpha);
}

float sampleGlow(vec2 uv)
{
    if (!glowEnabled || glowRadius <= 0.0 || glowIntensity <= 0.0)
        return 0.0;

    vec2 texel = vec2(1.0) / max(textureSize, vec2(1.0));
    float total = 0.0;
    float weightTotal = 0.0;
    for (int y = -4; y <= 4; ++y)
    {
        for (int x = -4; x <= 4; ++x)
        {
            vec2 point = vec2(float(x), float(y));
            float distanceWeight = exp(-dot(point, point) * 0.16);
            vec2 offset = point * (glowRadius * 0.25) * texel;
            total += combinedAlpha(clamp(uv + offset, 0.0, 1.0)) *
                     distanceWeight;
            weightTotal += distanceWeight;
        }
    }
    float blurred = total / max(weightTotal, 0.0001);
    return max(0.0, blurred - combinedAlpha(uv)) *
           clamp(glowIntensity, 0.0, 1.0);
}

vec4 over(vec4 foreground, vec4 background)
{
    float alpha = foreground.a + background.a * (1.0 - foreground.a);
    if (alpha <= 0.0001)
        return vec4(0.0);
    vec3 premultiplied = foreground.rgb * foreground.a +
                         background.rgb * background.a *
                         (1.0 - foreground.a);
    return vec4(premultiplied / alpha, alpha);
}

void main()
{
    vec2 uv = sf_TexCoord0.xy;
    vec4 fill = straightColor(texture2D(texture, uv));
    vec4 outline = straightColor(texture2D(outlineTexture, uv));

    if (gradientEnabled)
    {
        float minimum = gradientDirection == 1
                            ? contentMinimum.x
                            : contentMinimum.y;
        float maximum = gradientDirection == 1
                            ? contentMaximum.x
                            : contentMaximum.y;
        float coordinate = gradientDirection == 1 ? uv.x : 1.0 - uv.y;
        float percent = clamp(
            (coordinate - minimum) / max(maximum - minimum, 0.0001),
            0.0,
            1.0);
        vec4 gradientColor =
            texture2D(curveTexture, vec2(percent, 0.5));
        fill *= gradientColor;
    }

    vec4 glow = vec4(glowColor.rgb,
                     sampleGlow(uv) * glowColor.a);
    vec4 result = over(fill, over(outline, glow));
    result *= sf_FrontColor;
    gl_FragColor = result;
}
