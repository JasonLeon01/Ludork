uniform sampler2D texture;
uniform float lightBlock;

void main()
{
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
    float val = pixel.a * lightBlock;
    gl_FragColor = vec4(val, 0.0, 0.0, pixel.a);
}
