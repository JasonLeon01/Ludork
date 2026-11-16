#version 150 core
out vec4 fragmentColor;
in vec4 sf_FrontColor;
in vec4 sf_TexCoord0;

#define LUDORK_MAX_SHADER_LIGHTS 64

uniform float lightIntensity[LUDORK_MAX_SHADER_LIGHTS];


float GetLightIntensity(int index) {
    float intensity = 0.0;
    for (int i = 0; i < LUDORK_MAX_SHADER_LIGHTS; ++i) {
        if (i == index) {
            intensity = lightIntensity[i];
        }
    }
    return intensity;
}

void main() {
    vec2 offset = sf_TexCoord0.xy;
    float distanceToLight = length(offset);
    if (distanceToLight >= 1.0) {
        discard;
    }

    int index = int(floor(sf_FrontColor.a * 255.0 + 0.5));
    float radialAttenuation = 1.0 - distanceToLight;
    vec3 direct =
        sf_FrontColor.rgb *
        GetLightIntensity(index) *
        radialAttenuation;
    fragmentColor = vec4(direct, 1.0);
}
