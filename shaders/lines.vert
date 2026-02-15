#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor; // stored in Vertex.normal

layout(location = 0) out vec3 vColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 vp;
    mat4 model;      // unused here
    vec4 lightDir;   // unused
    vec4 baseColor;  // unused
    vec4 cameraPos;  // unused
    vec4 spec;       // unused
} ubo;

void main() {
    vColor = inColor;
    gl_Position = ubo.vp * vec4(inPos, 1.0);
}
