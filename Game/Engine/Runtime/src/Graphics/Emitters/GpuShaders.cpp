#include "GpuShaders.hpp"
#include <Runtime/Graphics/GpuEmitterCurveLayout.hpp>

namespace ludork::runtime::graphics {
namespace {
std::string vertexHeader(bool embedded) {
    std::string result =
        embedded ? "#version 300 es\nprecision highp float;\n#define IN "
                   "in\n#define OUT out\n#define TEX texture\n"
                 : "#version 120\n#define IN attribute\n#define OUT "
                   "varying\n#define TEX texture2D\n";
    result += "#define LUDORK_EMITTER_CURVE_SAMPLES " +
              std::to_string(emitter_curve_layout::SampleCount) + ".0\n";
    result += "#define LUDORK_EMITTER_CURVE_ROWS " +
              std::to_string(emitter_curve_layout::TextureRowCount) + ".0\n";
    return result;
}
const char* attributes = R"GLSL(
IN vec4 a0; IN vec4 a1; IN vec4 a2; IN vec4 a3; IN vec4 a4; IN vec4 a5;
IN vec2 corner;
uniform sampler2D uCurves;
vec4 sampleCurves(float age, float rowIndex) {
    float lastSample = LUDORK_EMITTER_CURVE_SAMPLES - 1.0;
    float row = (rowIndex + 0.5) / LUDORK_EMITTER_CURVE_ROWS;
    float x = clamp(age, 0.0, 1.0) * lastSample;
    float left = floor(x);
    return mix(TEX(uCurves, vec2((left + 0.5) / LUDORK_EMITTER_CURVE_SAMPLES, row)),
               TEX(uCurves, vec2((min(left + 1.0, lastSample) + 0.5) / LUDORK_EMITTER_CURVE_SAMPLES, row)), fract(x));
}
)GLSL";
}  // namespace

std::string emitterUpdateShader(bool embedded) {
    return vertexHeader(embedded) + attributes + R"GLSL(
OUT vec4 s0; OUT vec4 s1; OUT vec4 s2; OUT vec4 s3; OUT vec4 s4; OUT vec4 s5;
uniform float uDelta, uReset, uSeed, uCapacity, uBirthStart, uBirthCount;
uniform float uResident, uLoop, uDraining, uWorld, uShape, uRadius, uInnerRadius;
uniform float uDirection, uSpread, uDamping, uRadial, uTangential;
uniform vec2 uLifetime, uSpeed, uSizeMin, uSizeMax, uRotation, uAngular, uExtent, uGravity;
uniform vec2 uShapeScale, uMotion;
uniform vec4 uColourMin, uColourMax;
uniform mat4 uHost;
float randomValue(inout float state) {
    state = mod(state * 251.0 + 13849.0, 65521.0);
    vec2 bytes = vec2(mod(state,256.0),floor(state/256.0)) / 256.0;
    vec3 mixed = fract(vec3(bytes,bytes.x+bytes.y)*vec3(13.17,17.71,23.43));
    float folded = dot(mixed,mixed.yzx+vec3(3.11,7.31,5.97));
    return fract((mixed.x+folded)*(mixed.y+folded)+mixed.z*11.13);
}
void main() {
    float id = corner.x;
    s0=a0; s1=a1; s2=a2; s3=a3; s4=a4; s5=a5;
    if (uReset > 0.5) {
        s0=vec4(0.0); s1=vec4(-1.0,1.0,0.0,0.0);
        s2=vec4(0.0,0.0,0.0,id); s3=vec4(0.0); s4=vec4(0.0); s5=vec4(1.0,0.0,0.0,1.0);
    } else {
        float birthIndex = mod(id - uBirthStart + uCapacity, uCapacity);
        bool born = birthIndex < uBirthCount;
        bool recycle = uResident > 0.5 && uLoop > 0.5 && uDraining < 0.5 && s1.x >= 0.0 && s1.x + uDelta >= s1.y;
        if (born || recycle) {
            float generation = born ? mod(uSeed + floor(id / 65521.0), 65521.0) : mod(s4.w + 1.0, 65521.0);
            float seed = mod(mod(id,65521.0) * 239.0 + generation,65521.0);
            float r1=randomValue(seed), r2=randomValue(seed), r3=randomValue(seed), r4=randomValue(seed);
            float r5=randomValue(seed), r6=randomValue(seed), r7=randomValue(seed), r8=randomValue(seed);
            float r9=randomValue(seed), r10=randomValue(seed), r11=randomValue(seed), r12=randomValue(seed);
            float r13=randomValue(seed), r14=randomValue(seed), r15=randomValue(seed), r16=randomValue(seed);
            float angle = r1 * 6.283185307;
            float radius = sqrt(mix(uShape > 3.5 ? uInnerRadius*uInnerRadius : 0.0, uRadius*uRadius, r2));
            vec2 p = vec2(0.0);
            if (uShape > 2.5) p = vec2(cos(angle),sin(angle)) * radius;
            else if (uShape > 1.5) p = vec2(r3-0.5,r4-0.5)*uExtent;
            else if (uShape > 0.5) p = vec2((r3-0.5)*uExtent.x,0.0);
            p *= uShapeScale;
            angle = (uDirection + (r5*2.0-1.0)*uSpread) * 0.01745329252;
            vec2 velocity = vec2(cos(angle),sin(angle))*mix(uSpeed.x,uSpeed.y,r6);
            s5=vec4(1.0,0.0,0.0,1.0);
            if (uWorld > 0.5) {
                p = (uHost*vec4(p,0.0,1.0)).xy;
                if (born) p -= uMotion * (1.0 - (birthIndex + 0.5) / max(uBirthCount,1.0));
                velocity = (uHost*vec4(velocity,0.0,0.0)).xy;
                s5 = vec4(uHost[0].xy,uHost[1].xy);
            }
            s0=vec4(p,velocity);
            s1=vec4(0.0,mix(uLifetime.x,uLifetime.y,r7),
                    mix(uRotation.x,uRotation.y,r8)*0.01745329252,
                    mix(uAngular.x,uAngular.y,r9)*0.01745329252);
            s2=vec4(mix(uSizeMin,uSizeMax,vec2(r10,r11)),r12,id);
            s3=mix(uColourMin,uColourMax,vec4(r13,r14,r15,r16));
            s4=vec4(uWorld > 0.5 ? (uHost*vec4(0.0,0.0,0.0,1.0)).xy : vec2(0.0),seed,generation);
        }
        if (s1.x >= 0.0) {
            float dt = uDelta;
            if (born) dt *= 1.0 - (birthIndex + 0.5) / max(uBirthCount,1.0);
            s1.x += dt;
            if (s1.x >= s1.y && uResident > 0.5 && uLoop > 0.5 && uDraining < 0.5) s1.x=mod(s1.x,s1.y);
            if (s1.x >= s1.y) s1.x = -1.0;
            else {
                vec2 radial = s0.xy-s4.xy;
                float distance = length(radial);
                radial = distance > 0.0001 ? radial/distance : vec2(0.0);
                s0.zw += (uGravity + radial*uRadial + vec2(-radial.y,radial.x)*uTangential)*dt;
                s0.zw *= exp(-uDamping*dt);
                s0.xy += s0.zw * sampleCurves(s1.x/s1.y,0.0).x * dt;
                s1.z += s1.w * dt;
            }
        }
    }
    gl_Position=vec4(0.0,0.0,0.0,1.0);
}
)GLSL";
}

std::string emitterDrawShader(bool embedded) {
    return vertexHeader(embedded) + attributes + R"GLSL(
OUT vec2 vUv;
OUT vec4 vColour;
uniform mat4 uProjection, uHost;
uniform float uWorld, uFrameRate, uFrameCount, uFrameLoop, uRandomFrame;
uniform vec2 uGrid;
uniform vec4 uRect;
uniform vec4 uTint;
void main() {
    vec4 transformCurve=sampleCurves(a1.x/a1.y,0.0);
    float rotation=a1.z + transformCurve.w*0.01745329252;
    vec2 point=corner*a2.xy*transformCurve.yz;
    point=mat2(cos(rotation),sin(rotation),-sin(rotation),cos(rotation))*point;
    vec2 position;
    if (uWorld > 0.5) position=a0.xy+mat2(a5.xy,a5.zw)*point;
    else position=(uHost*vec4(a0.xy+point,0.0,1.0)).xy;
    gl_Position=a1.x < 0.0 ? vec4(2.0,2.0,0.0,1.0) : uProjection*vec4(position,0.0,1.0);
    float frame=floor(a1.x*uFrameRate)+(uRandomFrame>0.5?floor(a2.z*uFrameCount):0.0);
    frame=uFrameLoop>0.5?mod(frame,uFrameCount):min(frame,uFrameCount-1.0);
    vec2 cell=vec2(mod(frame,uGrid.x),floor(frame/uGrid.x));
    vUv=uRect.xy+(cell+corner+vec2(0.5))/uGrid*uRect.zw;
    vColour=a3*sampleCurves(a1.x/a1.y,1.0)*uTint;
}
)GLSL";
}

std::string emitterFragmentShader(bool embedded, bool textured) {
    std::string result =
        embedded ? "#version 300 es\nprecision highp float;\n#define IN "
                   "in\n#define TEX texture\nout vec4 resultColour;\n#define "
                   "RESULT resultColour\n"
                 : "#version 120\n#define IN varying\n#define TEX "
                   "texture2D\n#define RESULT gl_FragColor\n";
    result += textured
                  ? "IN vec2 vUv; IN vec4 vColour; uniform sampler2D uImage; "
                    "void main(){ RESULT=TEX(uImage,vUv)*vColour; }\n"
                  : "void main(){RESULT=vec4(0.0);}\n";
    return result;
}
}  // namespace ludork::runtime::graphics
