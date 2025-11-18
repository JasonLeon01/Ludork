uniform sampler2D screenTex;
uniform float intensity;

void main()
{
    vec2 texSize = vec2(textureSize(screenTex, 0));
    vec2 uv = gl_FragCoord.xy / texSize;
    vec3 c = texture(screenTex, clamp(uv, 0.0, 1.0)).rgb;
    float a = clamp(intensity, 0.0, 1.0);
    float g = dot(c, vec3(0.299, 0.587, 0.114));
    vec3 outc = mix(c, vec3(g), a);
    gl_FragColor = vec4(outc, 1.0);
}