#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 params;
} ubo;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragNormal;

void main() {
    mat3 normalMatrix = mat3(ubo.model);

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
    fragUV = inUV;
    fragNormal = normalize(normalMatrix * inNormal);
}