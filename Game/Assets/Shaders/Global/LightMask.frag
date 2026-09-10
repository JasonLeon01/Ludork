#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif

uniform sampler2D texture;
uniform float lightBlock;
uniform float reflectionStrength;
uniform float ignoreLighting;
uniform float transmissionMode;

void main()
{
    vec4 pixel = texture2D(texture, sf_TexCoord0.xy);
    float alpha = clamp(pixel.a * sf_FrontColor.a, 0.0, 1.0);
    float block = clamp(lightBlock, 0.0, 1.0);
    float maskValue = block;
    if (transmissionMode > 0.5)
        maskValue = 1.0 - alpha * block;
    gl_FragColor = vec4(
        maskValue,
        clamp(reflectionStrength, 0.0, 1.0),
        clamp(ignoreLighting, 0.0, 1.0),
        alpha
    );
}
