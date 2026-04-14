uniform sampler2D texture;
uniform sampler2D lightMask;
uniform sampler2D mirrorTex;
uniform sampler2D reflectionStrengthTex;

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

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float GetObstructionAttenuation(vec2 from, vec2 to, vec2 gridSize) {
    vec2 fromGrid = from / float(cellSize);
    vec2 toGrid = to / float(cellSize);
    vec2 delta = toGrid - fromGrid;

    int steps = int(clamp(max(abs(delta.x), abs(delta.y)) * 1.5, 8.0, 128.0));
    if (steps <= 0) {
        return 1.0;
    }

    vec2 step = delta / float(steps);
    float jitter = hash(gl_FragCoord.xy);
    vec2 curr = fromGrid + step * jitter;

    float attenuation = 0.0;

    for (int i = 0; i < 128; ++i) {
        if (i >= steps) {
            break;
        }
        vec2 sampleUV = curr / gridSize;
        float r = texture2D(lightMask, vec2(sampleUV.x, 1.0 - sampleUV.y)).r;
        attenuation = max(attenuation, r * 0.8);
        curr += step;
    }

    return clamp(1.0 - attenuation, 0.0, 1.0);
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
            vec4 refTex = texture2D(texture, reflectedScreenUV);
            float strength = texture2D(reflectionStrengthTex, tileCenterUV).r;
            float reflectionAlpha = refTex.a * strength;
            vec3 mirrorColor = mix(refTex.rgb, pixelColor, 0.05);
            return mix(pixelColor, mirrorColor, reflectionAlpha);
        }
    }
    return pixelColor;
}

vec3 CalculateLighting(vec2 pixelPosTL_world) {
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
    vec2 pixelPosTL_world = center + rotate2D(pixelPosTL_view - screenSize * 0.5, rad);
    vec2 pixelPosBL_world = center + rotate2D(pixelPosBL_view - screenSize * 0.5, rad);

    vec2 uv = clamp(gl_TexCoord[0].xy, 0.0, 1.0);
    vec3 pixelColor = texture2D(texture, uv).rgb;

    pixelColor = ApplyReflection(pixelColor, pixelPosBL_world, uv);
    vec3 lighting = CalculateLighting(pixelPosTL_world);

    gl_FragColor = vec4(pixelColor * lighting, 1.0);
}
