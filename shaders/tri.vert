#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNrm;
layout(location=2) in vec2 inUV;

layout(set=0,binding=0) uniform UBO {
    mat4 vp;
    mat4 model;
    vec4 lightDir;
    vec4 baseColor;
    vec4 cameraPos;
    vec4 spec;
} ubo;

layout(location=0) out vec3 vNrm;
layout(location=1) out vec2 vUV;

void main() {
    vec4 wpos = ubo.model * vec4(inPos, 1.0);
    gl_Position = ubo.vp * wpos;

    vNrm = normalize((ubo.model * vec4(inNrm, 0.0)).xyz);
    vUV  = inUV;
}
