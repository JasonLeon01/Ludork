#version 150 core
out vec4 fragmentColor;
in vec4 sf_TexCoord0;

vec4 sampleTexture(sampler2D image, vec2 uv) { return texture(image, uv); }
uniform sampler2D texture;
uniform sampler2D ambientLight;
uniform vec2 pixelSize;

void main() {
    vec4 source = sampleTexture(texture, sf_TexCoord0.xy);
    vec3 ambient = sampleTexture(ambientLight, gl_FragCoord.xy / pixelSize).rgb;
    fragmentColor = vec4(source.rgb * ambient, source.a);
}
