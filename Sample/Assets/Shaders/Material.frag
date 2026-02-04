uniform sampler2D tilemapTex;
uniform sampler2D blockableTex;
uniform sampler2D lightBlockTex;
uniform sampler2D mirrorTex;
uniform sampler2D reflectionStrengthTex;
uniform sampler2D emissiveTex;

uniform int lightCount;
uniform int cellSize;

uniform vec2 lightPos[16];
uniform vec3 lightColor[16];
uniform float lightRadius[16];
uniform float lightIntensity[16];

uniform vec3 ambientColor;
uniform float screenScale;
uniform vec2 screenSize;
uniform vec2 viewPos;

uniform vec2 gridSize;

float GetObstructionAttenuation(vec2 from, vec2 to, vec2 gridSize)
{
    vec2 fromGrid = from / float(cellSize);
    vec2 toGrid = to / float(cellSize);
    vec2 delta = toGrid - fromGrid;

    int steps = int(clamp(max(abs(delta.x), abs(delta.y)) * 1.2, 8.0, 64.0));
    if (steps <= 0) return 1.0;

    vec2 step = delta / float(steps);
    vec2 curr = fromGrid;

    float attenuation = 1.0;

    for (int i = 0; i < 64; ++i) {
        if (i >= steps) break;

        vec2 uv = clamp(curr / gridSize, 0.0, 1.0);
        float r = texture2D(lightBlockTex, uv).r;

        attenuation *= (1.0 - r * 0.6);
        curr += step;
    }

    return clamp(attenuation, 0.0, 1.0);
}

vec3 ApplyReflection(vec3 pixelColor, vec2 pixelPosBL_world, vec2 uv)
{
    vec2 gridIndex = floor(pixelPosBL_world / float(cellSize));
    vec2 tileCenterUV = (gridIndex + 0.5) / gridSize;
    float isMirror = texture2D(mirrorTex, tileCenterUV).r;

    if (isMirror > 0.5) {
        vec2 gridIndexAbove = gridIndex + vec2(0.0, 1.0);
        if (gridIndexAbove.y < gridSize.y) {
            vec2 tileCenterUVAbove = (gridIndexAbove + 0.5) / gridSize;
            float isAboveMirror = texture2D(mirrorTex, tileCenterUVAbove).r;
            if (isAboveMirror < 0.5) {
                vec2 cellLocal = fract(pixelPosBL_world / float(cellSize));
                vec2 reflectedLocal = vec2(cellLocal.x, 1.0 - cellLocal.y);
                vec2 reflectedWorldPosBL = (gridIndexAbove + reflectedLocal) * float(cellSize);
                vec2 deltaWorld = reflectedWorldPosBL - pixelPosBL_world;
                vec2 deltaUV = deltaWorld / screenSize;
                vec2 reflectedScreenUV = uv + deltaUV;

                if (reflectedScreenUV.x >= 0.0 && reflectedScreenUV.x <= 1.0 &&
                    reflectedScreenUV.y >= 0.0 && reflectedScreenUV.y <= 1.0) {
                    vec4 refTex = texture2D(blockableTex, reflectedScreenUV);
                    float strength = texture2D(reflectionStrengthTex, tileCenterUV).r;
                    float reflectionAlpha = refTex.a * strength;
                    return mix(pixelColor, refTex.rgb, reflectionAlpha);
                }
            }
        }
    }
    return pixelColor;
}

vec3 CalculateLighting(vec2 pixelPosTL_world, vec2 pixelPosBL_world, vec2 mapPixelSize)
{
    vec3 totalLight = ambientColor;

    for (int i = 0; i < 16; ++i) {
        if (i >= lightCount) break;

        float dist = length(pixelPosTL_world - lightPos[i]);
        if (dist >= lightRadius[i]) continue;

        float atten = 1.0 - dist / lightRadius[i];
        vec2 lightPosBL_world = vec2(lightPos[i].x, mapPixelSize.y - lightPos[i].y);
        float obs = GetObstructionAttenuation(lightPosBL_world, pixelPosBL_world, gridSize);

        totalLight += lightColor[i] * lightIntensity[i] * atten * obs;
    }
    return clamp(totalLight, 0.0, 1.0);
}

void main()
{
    vec2 pixelPosBL_view = gl_FragCoord.xy / screenScale;
    vec2 pixelPosTL_view = vec2(pixelPosBL_view.x, screenSize.y - pixelPosBL_view.y);
    vec2 pixelPosTL_world = pixelPosTL_view + viewPos;
    vec2 mapPixelSize = gridSize * float(cellSize);
    vec2 pixelPosBL_world = vec2(pixelPosTL_world.x, mapPixelSize.y - pixelPosTL_world.y);

    vec2 uv = clamp(gl_TexCoord[0].xy, 0.0, 1.0);
    vec3 pixelColor = texture2D(tilemapTex, uv).rgb;

    pixelColor = ApplyReflection(pixelColor, pixelPosBL_world, uv);
    vec3 lighting = CalculateLighting(pixelPosTL_world, pixelPosBL_world, mapPixelSize);

    vec2 gridIndex = floor(pixelPosBL_world / float(cellSize));
    vec2 tileCenterUV = (gridIndex + 0.5) / gridSize;
    float emissive = texture2D(emissiveTex, tileCenterUV).r;
    
    vec4 blockColor = texture2D(blockableTex, uv);
    vec3 emissiveColor = blockColor.rgb * blockColor.a * emissive;

    gl_FragColor = vec4(pixelColor * lighting + emissiveColor, 1.0);
}
