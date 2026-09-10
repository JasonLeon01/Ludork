attribute vec4 sf_Vertex;
attribute vec4 sf_Color;
attribute vec4 sf_MultiTexCoord0;

uniform mat4 sf_ModelViewProjectionMatrix;
uniform mat4 sf_TextureMatrix;
uniform float elapsed;
uniform float duration;
uniform float staggerDuration;
uniform vec2 snapshotOrigin;
uniform vec2 snapshotSize;
uniform float seed;

#ifdef GL_ES
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
varying mediump float shatterProgress;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
varying float shatterProgress;
#endif

float pixelHash(vec2 pixel, float salt)
{
    vec2 seeded = pixel
        + vec2(seed * 0.754877666, seed * 1.324717957)
        + vec2(salt, salt * 1.618033989);
    return fract(sin(dot(seeded, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 pixel = floor(sf_MultiTexCoord0.xy);
    vec2 localCenter = pixel + vec2(0.5);
    vec2 relative = localCenter - snapshotSize * 0.5;
    vec2 direction;
    if (dot(relative, relative) > 0.000001) {
        direction = normalize(relative);
    } else {
        float angle = pixelHash(pixel, 7.13) * 6.28318530718;
        direction = vec2(cos(angle), sin(angle));
    }

    float delay = pixelHash(pixel, 0.0) * staggerDuration;
    float localDuration = max(duration - delay, 0.0001);
    float progress = clamp((elapsed - delay) / localDuration, 0.0, 1.0);
    float movementProgress = 1.0 - pow(1.0 - progress, 3.0);
    float shrink = 1.0 - smoothstep(0.58, 1.0, progress);
    float distanceRandom = mix(0.8, 1.2, pixelHash(pixel, 19.19));
    float distance = max(snapshotSize.x, snapshotSize.y) * 0.75 * distanceRandom;
    vec2 worldCenter = snapshotOrigin + localCenter;
    vec2 corner = sf_Vertex.xy - worldCenter;

    vec4 vertex = sf_Vertex;
    vertex.xy = worldCenter
        + corner * shrink
        + direction * distance * movementProgress;
    gl_Position = sf_ModelViewProjectionMatrix * vertex;
    sf_TexCoord0 = sf_TextureMatrix * sf_MultiTexCoord0;
    sf_FrontColor = sf_Color;
    shatterProgress = progress;
}
