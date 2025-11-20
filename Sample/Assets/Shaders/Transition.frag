uniform sampler2D screenTex;
uniform sampler2D backTex;
uniform sampler2D transitionResource;
uniform int useMask;
uniform float progress;
uniform float totalTime;

void main()
{
    float t = totalTime <= 0.0 ? 1.0 : clamp(progress / totalTime, 0.0, 1.0);

    vec2 uv = gl_TexCoord[0].xy;
    vec3 front = texture2D(screenTex, clamp(uv, 0.0, 1.0)).rgb;
    vec3 back = texture2D(backTex, clamp(uv, 0.0, 1.0)).rgb;

    float a = t;
    if (useMask != 0) {
        vec3 maskColor = texture2D(transitionResource, clamp(uv, 0.0, 1.0)).rgb;
        float mask = dot(maskColor, vec3(0.299, 0.587, 0.114));
        float edge = 0.02;
        a = smoothstep(mask - edge, mask + edge, t);
    }
    vec3 outc = mix(back, front, a);
    gl_FragColor = vec4(outc, 1.0);
}