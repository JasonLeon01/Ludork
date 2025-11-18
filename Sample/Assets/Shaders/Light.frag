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
    // 格子坐标
    vec2 fromGrid = floor(from * 1.0 / cellSize);
    vec2 toGrid = floor(to * 1.0 / cellSize);
    vec2 delta = toGrid - fromGrid;
    int steps = int(max(abs(delta.x), abs(delta.y)));
    float attenuation = 1.0;

    if (steps == 0) return attenuation;

    // 步进
    vec2 step = delta / float(steps);
    vec2 curr = fromGrid;

    for (int i = 0; i <= 128; ++i) {
        // 防止溢出
        if (i > steps) break;

        // 查passability
        vec2 texUV = (floor(curr) + 0.5) / passabilityTexSize;
        texUV.y = 1.0 - texUV.y;
        float r = texture(passabilityTex, texUV).r;
        attenuation -= r;

        curr += step;
    }

    return max(attenuation, 0.0);
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
    float dist = length(pixelPos - lightPos[0]);
    if (dist < lightRadius[0]) {
        float attenuation = 1.0 - dist / lightRadius[0];
        attenuation = clamp(attenuation, 0.0, 1.0);
        float obsAtten = GetObstructionAttenuation(lightPos[0], pixelPos, passabilityTex, passabilityTexSize);
        attenuation *= obsAtten;
        totalLight += lightColor[0] * lightIntensity[0] * attenuation;
    }

    vec3 finalColor = pixelColor * clamp(totalLight, 0.0, 1.0);
    gl_FragColor = vec4(finalColor, 1.0);
}
