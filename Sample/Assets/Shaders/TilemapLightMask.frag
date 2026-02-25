uniform sampler2D texture;
uniform sampler2D lightBlockTex; 
uniform vec2 lightBlockSize; 
uniform vec2 mapSize;

void main()
{
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
    if (pixel.a <= 0.0)
        discard;
    
    vec2 totalPixelSize = mapSize * lightBlockSize;
    vec2 worldPos = gl_FragCoord.xy;
    worldPos.y = totalPixelSize.y - worldPos.y;
    
    vec2 cellUV = worldPos / totalPixelSize;
    float blockVal = texture2D(lightBlockTex, cellUV).r;
    float val = pixel.a * blockVal;
    if (val <= 0.0)
        discard;
    gl_FragColor = vec4(val, 0.0, 0.0, pixel.a);
}
