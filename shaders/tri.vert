#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vNormalW;
layout(location = 1) out vec3 vPosW;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
    vec4 baseColor;
    vec4 cameraPos;
    vec4 spec; // x=specStrength, y=shininess
} ubo;

void main() {
    vec4 posW = ubo.model * vec4(inPos, 1.0);
    vPosW = posW.xyz;

    // OK for our model (rotation/translation). If you later add scaling, use normal matrix.
    vNormalW = normalize(mat3(ubo.model) * inNormal);

    gl_Position = ubo.mvp * vec4(inPos, 1.0);
}
