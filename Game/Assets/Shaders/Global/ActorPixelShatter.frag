#version 150 core
out vec4 fragmentColor;
in vec4 sf_FrontColor;
in vec4 sf_TexCoord0;
in float shatterProgress;

vec4 sampleTexture(sampler2D image, vec2 uv) { return texture(image, uv); }
uniform sampler2D texture;

#define texture sampleTexture
// LUDORK_SCENE_LIGHTING
#undef texture

void main()
{
    vec4 pixel = sampleTexture(texture, sf_TexCoord0.xy);
    if (pixel.a > 0.000001) {
        pixel.rgb /= pixel.a;
    } else {
        pixel.rgb = vec3(0.0);
    }
    pixel *= sf_FrontColor;
    pixel.a *= 1.0 - smoothstep(0.58, 1.0, shatterProgress);
    fragmentColor = vec4(applySceneLighting(pixel.rgb), pixel.a);
}
