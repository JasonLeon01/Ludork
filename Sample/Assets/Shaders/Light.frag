uniform sampler2D tilemapTex;
uniform sampler2D passabilityTex;

uniform int lightCount;
uniform vec2 lightPos[16];
uniform vec3 lightColor[16];
uniform float lightRadius[16];
uniform float lightIntensity[16];

uniform vec3 ambientColor;

void main()
{
    vec2 texUV = gl_TexCoord[0].xy;
    vec2 fragPos = texUV * vec2(textureSize(tilemapTex,0));
    vec4 baseColor = texture(tilemapTex, texUV);
    float passValue = texture(passabilityTex, fragPos / vec2(textureSize(passabilityTex,0))).r; // 0~1
    vec3 lighting = ambientColor;
    for(int i=0;i<lightCount;i++)
    {
        float dist = distance(fragPos, lightPos[i]);
        float atten = clamp(1.0 - dist / lightRadius[i], 0.0, 1.0);
        float factor = lightIntensity[i] * atten;

        factor *= 1.0;

        lighting += lightColor[i] * factor;
    }
    lighting = clamp(lighting, 0.0, 1.0);
    gl_FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
