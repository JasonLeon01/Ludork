uniform sampler2D texture;
uniform float lightBlock;
uniform float reflectionStrength;
uniform float ignoreLighting;

void main()
{
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
    float val = pixel.a * lightBlock;
    float unlit = pixel.a * ignoreLighting;
    gl_FragColor = vec4(val, reflectionStrength, unlit, pixel.a);
}
