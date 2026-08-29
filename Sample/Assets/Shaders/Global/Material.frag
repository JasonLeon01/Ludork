#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_TexCoord0;
#endif

uniform sampler2D texture;
uniform sampler2D surfaceMask;
uniform sampler2D directLight;
uniform sampler2D staticDirectLight;
uniform float useStaticDirectLight;

uniform vec2 screenSize;
uniform vec2 viewPos;
uniform vec2 viewSinCos;
uniform vec2 gridSize;
uniform float cellSize;

uniform vec3 ambientColor;


vec2 rotate2D(vec2 v, vec2 sinCos) {
    return vec2(
        v.x * sinCos.y - v.y * sinCos.x,
        v.x * sinCos.x + v.y * sinCos.y
    );
}

vec2 ToMaskUV(vec2 worldPosTL) {
    vec2 worldSize = gridSize * float(cellSize);
    vec2 uv = worldPosTL / worldSize;
    return vec2(clamp(uv.x, 0.0, 1.0), clamp(1.0 - uv.y, 0.0, 1.0));
}

vec4 SampleSurfaceMaskWorldTL(vec2 worldPosTL) {
    return texture2D(surfaceMask, ToMaskUV(worldPosTL));
}

vec2 WorldTLToSceneUV(vec2 worldPosTL, vec2 center) {
    vec2 inverseSinCos = vec2(-viewSinCos.x, viewSinCos.y);
    vec2 viewPosTL = screenSize * 0.5 + rotate2D(worldPosTL - center, inverseSinCos);
    return vec2(
        clamp(viewPosTL.x / screenSize.x, 0.0, 1.0),
        clamp(1.0 - viewPosTL.y / screenSize.y, 0.0, 1.0)
    );
}

vec4 GetReflectionSample(
    vec2 pixelPosTLWorld,
    vec2 center,
    float reflectionStrength
) {
    reflectionStrength = clamp(reflectionStrength, 0.0, 1.0);
    if (reflectionStrength <= 0.0) {
        return vec4(0.0);
    }
    vec2 cellPos = floor(pixelPosTLWorld / float(cellSize));
    if (cellPos.y <= 0.0) {
        return vec4(0.0);
    }
    vec2 localPos = fract(pixelPosTLWorld / float(cellSize));

    vec2 reflectionSourceCell = cellPos + vec2(0.0, -1.0);
    vec2 sourceLocal = vec2(localPos.x, 1.0 - localPos.y);
    vec2 sourceWorldPosTL =
        (reflectionSourceCell + sourceLocal) * float(cellSize);
    float sourceBlock = clamp(
        SampleSurfaceMaskWorldTL(sourceWorldPosTL).r,
        0.0,
        1.0
    );
    float reflectAlpha = sourceBlock * reflectionStrength;
    if (reflectAlpha <= 0.0) {
        return vec4(0.0);
    }

    vec2 sourceUV = WorldTLToSceneUV(sourceWorldPosTL, center);
    vec4 sourceColor = texture2D(texture, sourceUV);
    return vec4(sourceColor.rgb, sourceColor.a * reflectAlpha);
}

void main() {
    vec2 uv = clamp(sf_TexCoord0.xy, 0.0, 1.0);
    vec2 pixelPosTLView =
        vec2(uv.x, 1.0 - uv.y) * screenSize;
    vec2 center = viewPos + screenSize * 0.5;
    vec2 pixelPosTLWorld =
        center + rotate2D(pixelPosTLView - screenSize * 0.5, viewSinCos);

    vec4 pixel = texture2D(texture, uv);
    vec3 direct;
    if (useStaticDirectLight > 0.5) {
        direct = texture2D(
            staticDirectLight,
            ToMaskUV(pixelPosTLWorld)
        ).rgb;
    } else {
        direct = texture2D(directLight, uv).rgb;
    }
    vec3 lighting = clamp(ambientColor + direct, 0.0, 1.0);
    vec3 litColor = pixel.rgb * lighting;

    vec4 surface = SampleSurfaceMaskWorldTL(pixelPosTLWorld);
    vec4 reflection = GetReflectionSample(
        pixelPosTLWorld,
        center,
        surface.g
    );
    vec3 reflectedColor = mix(litColor, reflection.rgb, reflection.a);
    float ignoreLighting = clamp(surface.b, 0.0, 1.0);
    vec3 finalColor = mix(reflectedColor, pixel.rgb, ignoreLighting);

    gl_FragColor = vec4(finalColor, 1.0);
}
