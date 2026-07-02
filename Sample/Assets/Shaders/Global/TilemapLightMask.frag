uniform sampler2D texture;
uniform sampler2D lightBlockTex;
uniform sampler2D reflectionStrengthTex;
uniform sampler2D ignoreLightingTex;
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
    float reflectionStrengthVal = texture2D(reflectionStrengthTex, cellUV).r;
    float ignoreLightingVal = texture2D(ignoreLightingTex, cellUV).r;
    float val = pixel.a * blockVal;
    float reflection = pixel.a * reflectionStrengthVal;
    float unlit = pixel.a * ignoreLightingVal;
    gl_FragColor = vec4(val, reflection, unlit, pixel.a);
}
