uniform int tilemapTexLen;
uniform sampler2D tilemapTex[16];
uniform int lightBlockLen;
uniform sampler2D lightBlockTex[16];
uniform sampler2D mirrorTex;
uniform sampler2D reflectionStrengthTex;
uniform sampler2D emissiveTex;

uniform float screenScale;
uniform vec2 screenSize;
uniform vec2 viewPos;
uniform float viewRot;
uniform vec2 gridSize;
uniform int cellSize;

uniform int lightLen;
uniform vec2 lightPos[16];
uniform vec3 lightColor[16];
uniform float lightRadius[16];
uniform float lightIntensity[16];

uniform vec3 ambientColor;


vec2 rotate2D(vec2 v, float a) {
    float s = sin(a);
    float c = cos(a);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

vec4 GetRealColor(vec2 uv) {
    vec4 blendedColor = vec4(0.0);
    for (int i = 0; i < 16; ++i) {
        if (i < tilemapTexLen) {
            vec4 currentColor = texture2D(tilemapTex[i], uv);
            blendedColor = vec4(currentColor.rgb * currentColor.a + blendedColor.rgb * (1.0 - currentColor.a), 
                                currentColor.a + blendedColor.a * (1.0 - currentColor.a));
        } else {
            break;
        }
    }
    return blendedColor;
}

vec2 ConvertUVToGridIndex(vec2 uv) {
    vec2 pixelPosTL_view = uv * screenSize;
    vec2 pixelPosBL_view = vec2(pixelPosTL_view.x, screenSize.y - pixelPosTL_view.y);
    float rad = viewRot * 0.017453292519943295;
    vec2 center = viewPos + screenSize * 0.5;
    vec2 pixelPosBL_world = center + rotate2D(pixelPosBL_view - screenSize * 0.5, rad);
    vec2 gridIndex = floor(pixelPosBL_world / float(cellSize));
    return gridIndex;
}

vec4 GetRealLightBlockColor(vec2 uv) {
    vec2 gridIndex = ConvertUVToGridIndex(uv);
    vec2 sampleUV = (gridIndex + 0.5) / gridSize;
    vec4 blendedColor = vec4(0.0);
    bool stopSkipping = false;
    for (int i = 0; i < 16; ++i) {
        if (i < tilemapTexLen && i < lightBlockLen) {
            float lightBlock = texture2D(lightBlockTex[i], sampleUV).r;
            if (lightBlock == 0.0 && !stopSkipping) {
                stopSkipping = true;
                continue;
            }
            vec4 currentColor = texture2D(tilemapTex[i], uv);
            blendedColor = vec4(currentColor.rgb * currentColor.a + blendedColor.rgb * (1.0 - currentColor.a), 
                                currentColor.a + blendedColor.a * (1.0 - currentColor.a));
        } else {
            break;
        }
    }
    return blendedColor;
}

float GetLightBlockValue(vec2 uv) {
    vec2 uv_ = clamp(uv / gridSize, 0.0, 1.0);
    for (int i = 15; i >= 0; --i) {
        if (i < tilemapTexLen && i < lightBlockLen) {
            float r = texture2D(lightBlockTex[i], uv_).r;
            if (r > 0.0) {
                return r;
            }
        }
    }
    return 0.0;
}

float GetObstructionAttenuation(vec2 from, vec2 to, vec2 gridSize) {
    vec2 fromGrid = from / float(cellSize);
    vec2 toGrid = to / float(cellSize);
    vec2 delta = toGrid - fromGrid;

    int steps = int(clamp(max(abs(delta.x), abs(delta.y)) * 1.2, 8.0, 64.0));
    if (steps <= 0) {
        return 1.0;
    }

    vec2 step = delta / float(steps);
    vec2 curr = fromGrid;

    float attenuation = 1.0;

    for (int i = 0; i < 64; ++i) {
        if (i >= steps) {
            break;
        }
        float r = GetLightBlockValue(curr);
        attenuation *= (1.0 - r * 0.6);
        curr += step;
    }

    return clamp(attenuation, 0.0, 1.0);
}

vec3 ApplyReflection(vec3 pixelColor, vec2 pixelPosBL_world, vec2 uv) {
    vec2 gridIndex = floor(pixelPosBL_world / float(cellSize));
    vec2 tileCenterUV = (gridIndex + 0.5) / gridSize;
    float isMirror = texture2D(mirrorTex, tileCenterUV).r;

    if (isMirror > 0.5) {
        float stepsUp = 0.0;
        for (int i = 1; i < 64; ++i) {
            vec2 checkIndex = gridIndex + vec2(0.0, float(i));
            if (checkIndex.y >= gridSize.y) {
                break;
            }
            vec2 checkUV = (checkIndex + 0.5) / gridSize;
            float checkMirror = texture2D(mirrorTex, checkUV).r;
            if (checkMirror < 0.5) {
                break;
            }
            stepsUp = float(i);
        }

        vec2 gridIndexTop = gridIndex + vec2(0.0, stepsUp);
        float topEdgeY = (gridIndexTop.y + 1.0) * float(cellSize);
        float deltaY = topEdgeY - pixelPosBL_world.y;

        vec2 reflectedWorldPosBL = vec2(pixelPosBL_world.x, topEdgeY + deltaY);
        vec2 deltaWorld = reflectedWorldPosBL - pixelPosBL_world;
        vec2 deltaUV = deltaWorld / screenSize;
        vec2 reflectedScreenUV = uv + deltaUV;

        if (reflectedScreenUV.x >= 0.0 && reflectedScreenUV.x <= 1.0 &&
            reflectedScreenUV.y >= 0.0 && reflectedScreenUV.y <= 1.0) {
            vec4 refTex = GetRealLightBlockColor(reflectedScreenUV);
            float strength = texture2D(reflectionStrengthTex, tileCenterUV).r;
            float reflectionAlpha = refTex.a * strength;
            vec3 mirrorColor = mix(refTex.rgb, pixelColor, 0.05);
            return mix(pixelColor, mirrorColor, reflectionAlpha);
        }
    }
    return pixelColor;
}

vec3 CalculateLighting(vec2 pixelPosTL_world, vec2 pixelPosBL_world, vec2 mapPixelSize) {
    vec3 totalLight = ambientColor;

    for (int i = 0; i < 16; ++i) {
        if (i >= lightLen) break;

        float dist = length(pixelPosTL_world - lightPos[i]);
        if (dist >= lightRadius[i]) continue;

        float atten = 1.0 - dist / lightRadius[i];
        float obs = GetObstructionAttenuation(lightPos[i], pixelPosTL_world, gridSize);

        totalLight += lightColor[i] * lightIntensity[i] * atten * obs;
    }
    return clamp(totalLight, 0.0, 1.0);
}

void main() {
    vec2 pixelPosBL_view = gl_FragCoord.xy / screenScale;
    vec2 pixelPosTL_view = vec2(pixelPosBL_view.x, screenSize.y - pixelPosBL_view.y);
    float rad = viewRot * 0.017453292519943295;
    vec2 center = viewPos + screenSize * 0.5;
    vec2 mapPixelSize = gridSize * float(cellSize);
    vec2 pixelPosTL_world = center + rotate2D(pixelPosTL_view - screenSize * 0.5, rad);
    vec2 pixelPosBL_world = center + rotate2D(pixelPosBL_view - screenSize * 0.5, rad);

    vec2 uv = clamp(gl_TexCoord[0].xy, 0.0, 1.0);
    vec3 pixelColor = GetRealColor(uv).rgb;

    pixelColor = ApplyReflection(pixelColor, pixelPosBL_world, uv);
    vec3 lighting = CalculateLighting(pixelPosTL_world, pixelPosBL_world, mapPixelSize);

    vec2 gridIndex = ConvertUVToGridIndex(uv);
    vec2 tileCenterUV = (gridIndex + 0.5) / gridSize;
    float emissive = texture2D(emissiveTex, tileCenterUV).r;
    if (emissive > 0.0) {
        vec4 blockColor = GetRealLightBlockColor(uv);
        vec3 emissiveColor = blockColor.rgb * blockColor.a * emissive;
        gl_FragColor = vec4(pixelColor * lighting + emissiveColor, 1.0);
    } else {
        gl_FragColor = vec4(pixelColor * lighting, 1.0);
    }
}
