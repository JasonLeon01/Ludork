uniform sampler2D screenTex;
uniform float intensity;

void main()
{
    vec2 texSize = vec2(textureSize(screenTex, 0));
    vec2 uv = gl_FragCoord.xy / texSize;
    if (intensity <= 0.0) {
        vec3 c = texture(screenTex, clamp(uv, 0.0, 1.0)).rgb;
        gl_FragColor = vec4(c, 1.0);
        return;
    }
    float k = intensity;
    vec2 t = k / texSize;

    vec3 sum = vec3(0.0);
    float count = 0.0;
    for (int j = -4; j <= 4; ++j) {
        for (int i = -4; i <= 4; ++i) {
            vec2 offs = t * vec2(float(i), float(j));
            sum += texture(screenTex, clamp(uv + offs, 0.0, 1.0)).rgb;
            count += 1.0;
        }
    }

    vec3 col = sum / count;
    gl_FragColor = vec4(col, 1.0);
}