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

vec4 sampleTextureRect(vec2 pixelPosition)
{
    vec2 rectSize = max(textureRect.zw, vec2(1.0));
    vec2 minimumPixel = textureRect.xy + vec2(0.5);
    vec2 maximumPixel = textureRect.xy + rectSize - vec2(0.5);
    if (
        pixelPosition.x < minimumPixel.x
        || pixelPosition.y < minimumPixel.y
        || pixelPosition.x > maximumPixel.x
        || pixelPosition.y > maximumPixel.y
    ) {
        return vec4(0.0);
    }

    return texture2D(texture, pixelPosition / max(textureSize, vec2(1.0)));
}

void main()
{
    vec2 pixelPosition = sf_TexCoord0.xy * textureSize;
    vec2 rectSize = max(textureRect.zw, vec2(1.0));
    vec2 localPixel = pixelPosition - textureRect.xy;
    vec2 localUv = clamp(localPixel / rectSize, 0.0, 1.0);

    float glitchCenterA = rectSize.y * (
        0.5 + 0.43 * sin(time * 0.41 + 0.7)
    );
    float glitchCenterB = rectSize.y * (
        0.5 + 0.44 * sin(time * 0.29 + 3.1)
    );
    float glitchBandA = 1.0 - smoothstep(
        0.25,
        0.9,
        abs(localPixel.y - glitchCenterA)
    );
    float glitchBandB = 1.0 - smoothstep(
        0.2,
        0.75,
        abs(localPixel.y - glitchCenterB)
    );
    float horizontalShift = clamp(
        glitchBandA * sin(time * 2.15 + 0.4) * 0.82
            + glitchBandB * sin(time * 1.73 + 2.2) * 0.38,
        -1.0,
        1.0
    );
    vec2 samplePixel = pixelPosition + vec2(horizontalShift, 0.0);

    vec4 source = sampleTextureRect(samplePixel);
    float leftAlpha = sampleTextureRect(samplePixel + vec2(-1.0, 0.0)).a;
    float rightAlpha = sampleTextureRect(samplePixel + vec2(1.0, 0.0)).a;
    float upperAlpha = sampleTextureRect(samplePixel + vec2(0.0, -1.0)).a;
    float lowerAlpha = sampleTextureRect(samplePixel + vec2(0.0, 1.0)).a;
    float upperLeftAlpha = sampleTextureRect(
        samplePixel + vec2(-1.0, -1.0)
    ).a;
    float upperRightAlpha = sampleTextureRect(
        samplePixel + vec2(1.0, -1.0)
    ).a;
    float lowerLeftAlpha = sampleTextureRect(
        samplePixel + vec2(-1.0, 1.0)
    ).a;
    float lowerRightAlpha = sampleTextureRect(
        samplePixel + vec2(1.0, 1.0)
    ).a;

    float cardinalMaximum = max(
        max(leftAlpha, rightAlpha),
        max(upperAlpha, lowerAlpha)
    );
    float diagonalMaximum = max(
        max(upperLeftAlpha, upperRightAlpha),
        max(lowerLeftAlpha, lowerRightAlpha)
    );
    float nearMaximum = max(cardinalMaximum, diagonalMaximum);
    float nearMinimum = min(
        min(leftAlpha, rightAlpha),
        min(upperAlpha, lowerAlpha)
    );
    float farMaximum = max(
        max(
            sampleTextureRect(samplePixel + vec2(-2.0, 0.0)).a,
            sampleTextureRect(samplePixel + vec2(2.0, 0.0)).a
        ),
        max(
            sampleTextureRect(samplePixel + vec2(0.0, -2.0)).a,
            sampleTextureRect(samplePixel + vec2(0.0, 2.0)).a
        )
    );

    float innerEdge = source.a * (1.0 - nearMinimum);
    float outerEdge = clamp(nearMaximum - source.a, 0.0, 1.0);
    float farHalo = clamp(
        farMaximum - max(source.a, nearMaximum),
        0.0,
        1.0
    );

    float scanPhase = abs(fract((localPixel.y - time * 3.0) * 0.25) - 0.5);
    float scanline = 1.0 - smoothstep(0.045, 0.17, scanPhase);
    float sweepCenter = 0.5 + 0.62 * sin(time * 0.31 - 1.0);
    float sweep = 1.0 - smoothstep(
        0.025,
        0.16,
        abs(localUv.y - sweepCenter)
    );
    float breathing = 0.5 + 0.5 * sin(time * 1.35);

    float luminance = dot(source.rgb, vec3(0.299, 0.587, 0.114));
    float projectionLight = clamp(luminance * 1.1 + 0.05, 0.0, 1.0);
    vec3 projectionColor = mix(
        vec3(0.025, 0.24, 0.34),
        vec3(0.52, 0.95, 1.0),
        projectionLight
    );
    vec3 bodyColor = mix(projectionColor, source.rgb, 0.14);
    bodyColor *= 0.92 + breathing * 0.06 + scanline * 0.12 + sweep * 0.12;

    float edgeLight = clamp(
        max(innerEdge * 1.25, outerEdge * 1.35),
        0.0,
        1.0
    );
    float haloLight = clamp(farHalo * 0.58, 0.0, 1.0);
    vec3 hologramColor = mix(
        bodyColor,
        vec3(0.72, 1.0, 1.0),
        max(edgeLight, haloLight)
    );
    hologramColor += vec3(0.07, 0.14, 0.16) * edgeLight;

    float bodyOpacity = 0.62
        + breathing * 0.07
        + scanline * 0.025
        + sweep * 0.025;
    float hologramAlpha = source.a * bodyOpacity;
    hologramAlpha = max(hologramAlpha, innerEdge * 0.82);
    hologramAlpha = max(hologramAlpha, outerEdge * 0.7);
    hologramAlpha = max(hologramAlpha, farHalo * 0.24);

    gl_FragColor = vec4(
        clamp(hologramColor, 0.0, 1.0),
        clamp(hologramAlpha, 0.0, 1.0)
    ) * sf_FrontColor;
}
