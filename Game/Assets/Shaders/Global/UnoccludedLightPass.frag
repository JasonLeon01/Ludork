#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif

uniform float lightIntensity[16];


float GetLightIntensity(int index) {
    float intensity = 0.0;
    for (int i = 0; i < 16; ++i) {
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
    gl_FragColor = vec4(direct, 1.0);
}
