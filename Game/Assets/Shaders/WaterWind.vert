attribute vec4 sf_Vertex;
attribute vec4 sf_Color;
attribute vec4 sf_MultiTexCoord0;

uniform mat4 sf_ModelViewProjectionMatrix;
uniform mat4 sf_TextureMatrix;
uniform float time;

#ifdef GL_ES
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif

void main()
{
    vec4 vertex = sf_Vertex;
    float broadCurrent = sin(vertex.y * 0.045 + time * 1.5);
    float crossingRipple = sin(vertex.y * 0.095 + vertex.x * 0.025 - time * 2.1);
    float fineRipple = sin(vertex.y * 0.19 + time * 2.8);

    vertex.x += broadCurrent * 1.8 + crossingRipple * 1.0 + fineRipple * 0.4;

    gl_Position = sf_ModelViewProjectionMatrix * vertex;
    sf_TexCoord0 = sf_TextureMatrix * sf_MultiTexCoord0;
    sf_FrontColor = sf_Color;
}
