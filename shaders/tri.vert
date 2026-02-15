#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vNormal;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
    vec4 baseColor;
} ubo;

void main() {
    // model has only rotation/translation in our code -> mat3(model) is OK
    vNormal = normalize(mat3(ubo.model) * inNormal);
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
}
