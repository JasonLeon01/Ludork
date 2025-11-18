uniform sampler2D screenTex;
uniform vec4 toneColor;

void main()
{
    vec2 texSize = vec2(textureSize(screenTex, 0));
    vec2 uv = gl_FragCoord.xy / texSize;
    vec3 src = texture(screenTex, clamp(uv, 0.0, 1.0)).rgb;
    vec3 tgt = clamp(toneColor.rgb, 0.0, 1.0);
    float a = clamp(toneColor.a, 0.0, 1.0);
    vec3 outc = mix(src, tgt, a);
    gl_FragColor = vec4(outc, 1.0);
}