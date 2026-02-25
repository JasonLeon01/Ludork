uniform sampler2D screenTex;
uniform vec2 texSize;
uniform int count;
uniform vec2 center0;
uniform vec2 center1;
uniform vec2 center2;
uniform vec2 center3;
uniform float radius0;
uniform float radius1;
uniform float radius2;
uniform float radius3;
uniform float thickness0;
uniform float thickness1;
uniform float thickness2;
uniform float thickness3;
uniform float strength0;
uniform float strength1;
uniform float strength2;
uniform float strength3;

vec2 getRoarOffset(vec2 uv, vec2 center, float radius, float thickness, float strength, float seed) {
    if (strength <= 0.0 || thickness <= 0.0 || radius <= 0.0) return vec2(0.0);
    
    vec2 d = uv - center;
    float d2 = dot(d, d);
    if (d2 <= 1e-12) return vec2(0.0);

    vec2 dPixel = d * texSize;
    float dist = length(dPixel);
    float angle = atan(dPixel.y, dPixel.x);
    
    float s1 = sin(angle * 20.0 + seed);
    float s2 = sin(angle * 47.0 + seed * 1.3);
    float s3 = sin(angle * 13.0 + seed * 2.7);
    float jagged = (s1 + s2 * 0.5 + s3 * 0.25); 
    
    float spikeAmp = thickness * 0.75;
    float distortedR = radius + jagged * spikeAmp;
    
    float x = (dist - distortedR) / thickness;
    float band = clamp(1.0 - abs(x), 0.0, 1.0);
    
    band = smoothstep(0.0, 1.0, band);
    band = pow(band, 0.8);
    
    return normalize(d) * (band * strength) / texSize;
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    
    if (count <= 0) {
        vec3 src = texture2D(screenTex, clamp(uv, 0.0, 1.0)).rgb;
        gl_FragColor = vec4(src, 1.0);
        return;
    }
    
    vec2 offs = vec2(0.0);
    
    if (count > 0) offs += getRoarOffset(uv, center0, radius0, thickness0, strength0, 0.0);
    if (count > 1) offs += getRoarOffset(uv, center1, radius1, thickness1, strength1, 10.0);
    if (count > 2) offs += getRoarOffset(uv, center2, radius2, thickness2, strength2, 20.0);
    if (count > 3) offs += getRoarOffset(uv, center3, radius3, thickness3, strength3, 30.0);

    vec3 col = texture2D(screenTex, clamp(uv + offs, 0.0, 1.0)).rgb;
    gl_FragColor = vec4(col, 1.0);
}
