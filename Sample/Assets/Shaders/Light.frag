uniform sampler2D tilemapTex;
uniform sampler2D passabilityTex;

uniform int lightCount;
uniform int cellSize;
uniform vec2 lightPos[16];
uniform vec3 lightColor[16];
uniform float lightRadius[16];
uniform float lightIntensity[16];

uniform vec3 ambientColor;
uniform float screenScale;

float GetObstructionAttenuation(vec2 from, vec2 to, sampler2D passabilityTex, vec2 passabilityTexSize) {
    vec2 fromGrid = from / float(cellSize);
    vec2 toGrid = to / float(cellSize);
    vec2 delta = toGrid - fromGrid;
    int baseSteps = int(max(abs(delta.x), abs(delta.y)));
    int steps = int(min(128.0, float(baseSteps) * 4.0));
    float attenuation = 1.0;
    if (steps <= 0) return attenuation;

    vec2 step = delta / float(steps);
    vec2 curr = fromGrid;
    int tailSkip = int(max(1.0, float(steps) / 4.0));
    int headSkip = int(max(0.0, float(steps) / 16.0));
    for (int i = 0; i < 128; ++i) {
        if (i >= steps - tailSkip) break;
        if (i < headSkip) { curr += step; continue; }
        vec2 texUV = curr / passabilityTexSize;
        texUV.y = 1.0 - texUV.y;
        float r = texture(passabilityTex, texUV).r;
        attenuation *= (1.0 - r);
        curr += step;
    }
    return clamp(attenuation, 0.0, 1.0);
}

void main()
{
    vec2 pixelPos = gl_FragCoord.xy / screenScale;
    vec2 tilemapTexSize = vec2(textureSize(tilemapTex, 0));
    vec2 passabilityTexSize = vec2(textureSize(passabilityTex, 0));

    vec2 uv = pixelPos / tilemapTexSize;
    uv = clamp(uv, vec2(0.0), vec2(1.0));
    vec3 pixelColor = texture(tilemapTex, uv).rgb;
    vec3 totalLight = ambientColor;
    for (int i = 0; i < 16; ++i) {
        if (i >= lightCount) break;
        vec2 reallightPos = vec2(lightPos[i].x, tilemapTexSize.y - lightPos[i].y);
        float dist = length(pixelPos - reallightPos);
        if (dist < lightRadius[i]) {
            float attenuation = 1.0 - dist / lightRadius[i];
            attenuation = clamp(attenuation, 0.0, 1.0);
            float obsAtten = GetObstructionAttenuation(reallightPos, pixelPos, passabilityTex, passabilityTexSize);
            attenuation *= obsAtten;
            totalLight += lightColor[i] * lightIntensity[i] * attenuation;
        }
    }

    vec3 finalColor = pixelColor * clamp(totalLight, 0.0, 1.0);
    gl_FragColor = vec4(finalColor, 1.0);
}
