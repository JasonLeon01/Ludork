uniform float time;

void main()
{
    vec4 vertex = gl_Vertex;
    float broadCurrent = sin(vertex.y * 0.045 + time * 1.5);
    float crossingRipple = sin(vertex.y * 0.095 + vertex.x * 0.025 - time * 2.1);
    float fineRipple = sin(vertex.y * 0.19 + time * 2.8);

    vertex.x += broadCurrent * 1.8 + crossingRipple * 1.0 + fineRipple * 0.4;

    gl_Position = gl_ModelViewProjectionMatrix * vertex;
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
}
