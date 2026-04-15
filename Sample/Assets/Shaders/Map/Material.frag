uniform sampler2D texture;
uniform sampler2D lightMask;

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

vec2 ToMaskUV(vec2 worldPosTL) {
    vec2 gridPos = worldPosTL / float(cellSize);
    vec2 uv = gridPos / gridSize;
    return vec2(clamp(uv.x, 0.0, 1.0), clamp(1.0 - uv.y, 0.0, 1.0));
}

vec4 SampleLightMaskWorldTL(vec2 worldPosTL) {
    return texture2D(lightMask, ToMaskUV(worldPosTL));
}

vec2 WorldTLToSceneUV(vec2 worldPosTL, vec2 center, float rad) {
    vec2 viewPosTL = screenSize * 0.5 + rotate2D(worldPosTL - center, -rad);
    return vec2(
        clamp(viewPosTL.x / screenSize.x, 0.0, 1.0),
        clamp(1.0 - viewPosTL.y / screenSize.y, 0.0, 1.0)
    );
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

vec4 GetReflectionSample(vec2 pixelPosTL_world, vec2 center, float rad) {
    vec2 cellPos = floor(pixelPosTL_world / float(cellSize));
    vec2 localPos = fract(pixelPosTL_world / float(cellSize));
    float reflectionStrength = SampleLightMaskWorldTL(pixelPosTL_world).g;
    if (reflectionStrength <= 0.0) {
        return vec4(0.0);
    }
    if (cellPos.y <= 0.0) {
        return vec4(0.0);
    }
    vec2 sourceCell = cellPos + vec2(0.0, -1.0);
    vec2 sourceLocal = vec2(localPos.x, 1.0 - localPos.y);
    vec2 sourceWorldPosTL = (sourceCell + sourceLocal) * float(cellSize);
    float sourceBlock = SampleLightMaskWorldTL(sourceWorldPosTL).r;
    float reflectAlpha = clamp(sourceBlock * reflectionStrength, 0.0, 1.0);
    if (reflectAlpha <= 0.0) {
        return vec4(0.0);
    }
    vec2 sourceUV = WorldTLToSceneUV(sourceWorldPosTL, center, rad);
    vec4 sourceColor = texture2D(texture, sourceUV);
    return vec4(sourceColor.rgb, sourceColor.a * reflectAlpha);
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

    vec3 lighting = CalculateLighting(pixelPosTL_world);
    vec3 baseColor = pixelColor * lighting;
    vec4 reflection = GetReflectionSample(pixelPosTL_world, center, rad);
    vec3 finalColor = mix(baseColor, reflection.rgb, reflection.a);

    gl_FragColor = vec4(finalColor, 1.0);
}
