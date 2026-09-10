#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif

uniform sampler2D texture;
uniform float time;
uniform vec2 textureSize;
uniform vec4 textureRect;

void main()
{
    vec2 uv = sf_TexCoord0.xy;
    vec4 texColor = texture2D(texture, uv);
    vec2 pixelPosition = uv * textureSize;
    vec2 localUv = clamp(
        (pixelPosition - textureRect.xy) / max(textureRect.zw, vec2(1.0)),
        0.0,
        1.0
    );

    float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
    vec3 coldColor = mix(vec3(luminance), vec3(0.37, 0.72, 0.94), 0.55);
    float edgeDistance = max(abs(localUv.x - 0.5), abs(localUv.y - 0.5)) * 2.0;
    float edgeFrost = smoothstep(0.46, 1.0, edgeDistance);
    float crackA = abs(localUv.x - 0.48 - sin(localUv.y * 17.0 + 0.7) * 0.075);
    float crackB = abs(localUv.y - 0.61 - sin(localUv.x * 21.0 + 1.4) * 0.055);
    float cracks = max(
        1.0 - smoothstep(0.0, 0.025, crackA),
        1.0 - smoothstep(0.0, 0.021, crackB)
    );
    float shimmer = 0.5 + 0.5 * sin(time * 3.1 + localUv.x * 19.0 - localUv.y * 13.0);
    float frost = clamp(edgeFrost * 0.48 + cracks * (0.42 + shimmer * 0.38), 0.0, 1.0);
    vec3 frozenColor = mix(coldColor, vec3(0.82, 0.96, 1.0), frost);
    frozenColor += vec3(0.08, 0.16, 0.22) * shimmer * cracks;

    gl_FragColor = vec4(clamp(frozenColor, 0.0, 1.0), texColor.a) * sf_FrontColor;
}
