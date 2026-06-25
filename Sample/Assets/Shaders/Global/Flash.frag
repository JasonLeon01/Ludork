uniform sampler2D screenTex;
uniform vec4 flashColor;
uniform float intensity;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec3 src = texture2D(screenTex, clamp(uv, 0.0, 1.0)).rgb;
    float a = clamp(flashColor.a, 0.0, 1.0) * clamp(intensity, 0.0, 1.0);
    vec3 add = clamp(flashColor.rgb, 0.0, 1.0) * a;
    vec3 outc = clamp(src + add, 0.0, 1.0);
    gl_FragColor = vec4(outc, 1.0);
}
