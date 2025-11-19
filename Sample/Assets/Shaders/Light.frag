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
uniform vec2 screenSize;
uniform vec2 viewPos;

float GetObstructionAttenuation(vec2 from, vec2 to, sampler2D passabilityTex, vec2 gridSize) 
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

        vec2 uv = curr / gridSize;
        float r = texture(passabilityTex, uv).r;

        attenuation *= (1.0 - r * 0.6);
        curr += step;
    }

    return clamp(attenuation, 0.0, 1.0);
}

void main()
{
    vec2 pixelPosBL_view = gl_FragCoord.xy / screenScale;
    vec2 pixelPosTL_view = vec2(pixelPosBL_view.x, screenSize.y - pixelPosBL_view.y);
    vec2 pixelPosTL_world = pixelPosTL_view + viewPos;
    vec2 gridSize = vec2(textureSize(passabilityTex, 0));
    vec2 mapPixelSize = gridSize * float(cellSize);
    vec2 pixelPosBL_world = vec2(pixelPosTL_world.x, mapPixelSize.y - pixelPosTL_world.y);
    vec2 tilemapSize = vec2(textureSize(tilemapTex, 0));

    vec2 uv = clamp(pixelPosBL_view / tilemapSize, 0.0, 1.0);
    vec3 pixelColor = texture(tilemapTex, uv).rgb;

    vec3 totalLight = ambientColor;

    for (int i = 0; i < lightCount; ++i) {

        float dist = length(pixelPosTL_world - lightPos[i]);
        if (dist >= lightRadius[i]) continue;

        float atten = 1.0 - dist / lightRadius[i];
        vec2 lightPosBL_world = vec2(lightPos[i].x, mapPixelSize.y - lightPos[i].y);
        float obs = GetObstructionAttenuation(lightPosBL_world, pixelPosBL_world, passabilityTex, gridSize);

        totalLight += lightColor[i] * lightIntensity[i] * atten * obs;
    }

    gl_FragColor = vec4(pixelColor * clamp(totalLight, 0.0, 1.0), 1.0);
}
