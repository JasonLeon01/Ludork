#include "GpuShaders.hpp"
#include "GpuParticleLayout.hpp"
#include "GpuParticleRandom.hpp"
#include <Runtime/Graphics/EmitterTrackParameters.hpp>
#include <Runtime/Graphics/GpuEmitterCurveLayout.hpp>
#include <Runtime/Graphics/SceneLighting.hpp>

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
    result += "#define LUDORK_EMITTER_SEED_MODULUS " +
              std::to_string(gpu_particle_random::SeedModulus) + ".0\n";
    result +=
        "#define LUDORK_EMITTER_SHAPE_LINE " +
        std::to_string(static_cast<int>(EmitterTrackParameters::Shape::Line)) +
        ".0\n";
    result += "#define LUDORK_EMITTER_SHAPE_RECTANGLE " +
              std::to_string(
                  static_cast<int>(EmitterTrackParameters::Shape::Rectangle)) +
              ".0\n";
    result +=
        "#define LUDORK_EMITTER_SHAPE_DISK " +
        std::to_string(static_cast<int>(EmitterTrackParameters::Shape::Disk)) +
        ".0\n";
    result +=
        "#define LUDORK_EMITTER_SHAPE_RING " +
        std::to_string(static_cast<int>(EmitterTrackParameters::Shape::Ring)) +
        ".0\n";
    return result;
}
std::string particleAttributes() {
    std::string result;
    for (const char* name : gpu_particle_layout::StateAttributeNames) {
        result += "IN vec4 " + std::string(name) + ";\n";
    }
    result += "IN vec2 " +
              std::string(gpu_particle_layout::CornerAttributeName) + ";\n";
    return result + R"GLSL(
const float DEGREES_TO_RADIANS = 0.01745329252;
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
}
std::string particleOutputs() {
    std::string result;
    for (const char* name : gpu_particle_layout::FeedbackNames) {
        result += "OUT vec4 " + std::string(name) + ";\n";
    }
    return result;
}
}  // namespace

std::string emitterUpdateShader(bool embedded) {
    return vertexHeader(embedded) + particleAttributes() + particleOutputs() +
           R"GLSL(
uniform float uDelta, uReset, uSeed, uCapacity, uBirthStart, uBirthCount;
uniform float uResident, uLoop, uDraining, uWorld, uShape, uRadius, uInnerRadius;
uniform float uDirection, uSpread, uDamping, uRadial, uTangential;
uniform vec2 uLifetime, uSpeed, uSizeMin, uSizeMax, uRotation, uAngular, uExtent, uGravity;
uniform vec2 uShapeScale, uMotion;
uniform vec4 uColourMin, uColourMax;
uniform mat4 uHost;
const float FULL_CIRCLE_RADIANS = 6.283185307;
const float RANDOM_STATE_MULTIPLIER = 251.0;
const float RANDOM_STATE_INCREMENT = 13849.0;
const float PARTICLE_ID_SEED_MULTIPLIER = 239.0;
const float RANDOM_BYTE_RADIX = 256.0;
const vec3 RANDOM_MIX_SCALES = vec3(13.17,17.71,23.43);
const vec3 RANDOM_FOLD_OFFSETS = vec3(3.11,7.31,5.97);
const float RANDOM_FOLD_SCALE = 11.13;
const float MINIMUM_RADIAL_DISTANCE = 0.0001;
float randomValue(inout float state) {
    state = mod(state * RANDOM_STATE_MULTIPLIER + RANDOM_STATE_INCREMENT, LUDORK_EMITTER_SEED_MODULUS);
    vec2 bytes = vec2(mod(state,RANDOM_BYTE_RADIX),floor(state/RANDOM_BYTE_RADIX)) / RANDOM_BYTE_RADIX;
    vec3 mixed = fract(vec3(bytes,bytes.x+bytes.y)*RANDOM_MIX_SCALES);
    float folded = dot(mixed,mixed.yzx+RANDOM_FOLD_OFFSETS);
    return fract((mixed.x+folded)*(mixed.y+folded)+mixed.z*RANDOM_FOLD_SCALE);
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
            float generation = born ? mod(uSeed + floor(id / LUDORK_EMITTER_SEED_MODULUS), LUDORK_EMITTER_SEED_MODULUS) : mod(s4.w + 1.0, LUDORK_EMITTER_SEED_MODULUS);
            float seed = mod(mod(id,LUDORK_EMITTER_SEED_MODULUS) * PARTICLE_ID_SEED_MULTIPLIER + generation,LUDORK_EMITTER_SEED_MODULUS);
            float r1=randomValue(seed), r2=randomValue(seed), r3=randomValue(seed), r4=randomValue(seed);
            float r5=randomValue(seed), r6=randomValue(seed), r7=randomValue(seed), r8=randomValue(seed);
            float r9=randomValue(seed), r10=randomValue(seed), r11=randomValue(seed), r12=randomValue(seed);
            float r13=randomValue(seed), r14=randomValue(seed), r15=randomValue(seed), r16=randomValue(seed);
            float angle = r1 * FULL_CIRCLE_RADIANS;
            float radius = sqrt(mix(uShape == LUDORK_EMITTER_SHAPE_RING ? uInnerRadius*uInnerRadius : 0.0, uRadius*uRadius, r2));
            vec2 p = vec2(0.0);
            if (uShape == LUDORK_EMITTER_SHAPE_DISK || uShape == LUDORK_EMITTER_SHAPE_RING) p = vec2(cos(angle),sin(angle)) * radius;
            else if (uShape == LUDORK_EMITTER_SHAPE_RECTANGLE) p = vec2(r3-0.5,r4-0.5)*uExtent;
            else if (uShape == LUDORK_EMITTER_SHAPE_LINE) p = vec2((r3-0.5)*uExtent.x,0.0);
            p *= uShapeScale;
            angle = (uDirection + (r5*2.0-1.0)*uSpread) * DEGREES_TO_RADIANS;
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
                    mix(uRotation.x,uRotation.y,r8)*DEGREES_TO_RADIANS,
                    mix(uAngular.x,uAngular.y,r9)*DEGREES_TO_RADIANS);
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
                radial = distance > MINIMUM_RADIAL_DISTANCE ? radial/distance : vec2(0.0);
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
    return vertexHeader(embedded) + particleAttributes() + R"GLSL(
OUT vec2 vUv;
OUT vec4 vColour;
uniform mat4 uProjection, uHost;
uniform float uWorld, uFrameRate, uFrameCount, uFrameLoop, uRandomFrame;
uniform vec2 uGrid;
uniform vec4 uRect;
uniform vec4 uTint;
void main() {
    vec4 transformCurve=sampleCurves(a1.x/a1.y,0.0);
    float rotation=a1.z + transformCurve.w*DEGREES_TO_RADIANS;
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
    if (textured) {
        result += sceneLightingGlsl("TEX");
        result +=
            "IN vec2 vUv; IN vec4 vColour; uniform sampler2D uImage; "
            "void main(){ vec4 pixel=TEX(uImage,vUv)*vColour; "
            "RESULT=vec4(applySceneLighting(pixel.rgb),pixel.a); }\n";
    } else {
        result += "void main(){RESULT=vec4(0.0);}\n";
    }
    return result;
}
}  // namespace ludork::runtime::graphics
