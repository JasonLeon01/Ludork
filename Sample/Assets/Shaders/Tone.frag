uniform sampler2D screenTex;
uniform vec4 toneColor;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec4 src = texture2D(screenTex, clamp(uv, 0.0, 1.0));
    vec3 shifted = clamp(src.rgb + toneColor.rgb, 0.0, 1.0);
    float gray = dot(shifted, vec3(0.299, 0.587, 0.114));
    vec3 outc = mix(shifted, vec3(gray), clamp(toneColor.a, 0.0, 1.0));
    gl_FragColor = vec4(outc, src.a);
}
