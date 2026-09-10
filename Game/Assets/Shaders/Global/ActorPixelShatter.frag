#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
varying mediump float shatterProgress;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
varying float shatterProgress;
#endif

uniform sampler2D texture;

void main()
{
    vec4 pixel = texture2D(texture, sf_TexCoord0.xy);
    if (pixel.a > 0.000001) {
        pixel.rgb /= pixel.a;
    } else {
        pixel.rgb = vec3(0.0);
    }
    pixel *= sf_FrontColor;
    pixel.a *= 1.0 - smoothstep(0.58, 1.0, shatterProgress);
    gl_FragColor = pixel;
}
