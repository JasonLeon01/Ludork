uniform sampler2D texture;
uniform float lightBlock;
uniform float reflectionStrength;

void main()
{
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
    float val = pixel.a * lightBlock;
    gl_FragColor = vec4(val, reflectionStrength, 0.0, pixel.a);
}
