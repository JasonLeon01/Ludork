uniform sampler2D texture;
uniform float time;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec4 texColor = texture2D(texture, uv);
    
    // Rainbow effect based on UV and time
    float speed = 2.0;
    float r = sin(uv.x * 3.14159 + time * speed) * 0.5 + 0.5;
    float g = sin(uv.x * 3.14159 + time * speed + 2.094) * 0.5 + 0.5;
    float b = sin(uv.x * 3.14159 + time * speed + 4.189) * 0.5 + 0.5;
    
    vec3 rainbow = vec3(r, g, b);
    
    // Mix with original texture color
    vec3 finalColor = mix(texColor.rgb, rainbow, 0.5);
    
    gl_FragColor = vec4(finalColor, texColor.a);
}
