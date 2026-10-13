#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_TexCoord0;
#endif

uniform sampler2D texture;
uniform sampler2D ambientLight;
uniform vec2 pixelSize;

void main() {
    vec4 source = texture2D(texture, sf_TexCoord0.xy);
    vec3 ambient = texture2D(ambientLight, gl_FragCoord.xy / pixelSize).rgb;
    gl_FragColor = vec4(source.rgb * ambient, source.a);
}
