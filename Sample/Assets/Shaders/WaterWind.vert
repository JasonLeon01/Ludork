uniform float time;

void main()
{
    vec4 vertex = gl_Vertex;
    float primaryWave = sin(vertex.x * 0.045 + time * 1.8);
    float secondaryWave = sin(vertex.x * 0.021 + vertex.y * 0.032 - time * 1.1);

    vertex.x += secondaryWave * 0.8;
    vertex.y += primaryWave * 1.6 + secondaryWave * 0.6;

    gl_Position = gl_ModelViewProjectionMatrix * vertex;
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
}
