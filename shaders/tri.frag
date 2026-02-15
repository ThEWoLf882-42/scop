#version 450

layout(location = 0) in vec3 vNormalW;
layout(location = 1) in vec3 vPosW;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 vp;
    mat4 model;
    vec4 lightDir;
    vec4 baseColor;
    vec4 cameraPos;
    vec4 spec; // x=specStrength, y=shininess
} ubo;

void main() {
    vec3 N = normalize(vNormalW);
    vec3 L = normalize(-ubo.lightDir.xyz);
    vec3 V = normalize(ubo.cameraPos.xyz - vPosW);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float specTerm = pow(max(dot(N, H), 0.0), ubo.spec.y) * ubo.spec.x;

    vec3 ambient = 0.15 * ubo.baseColor.rgb;
    vec3 color = ambient + diff * ubo.baseColor.rgb + specTerm * vec3(1.0);

    outColor = vec4(color, 1.0);
}
