#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
    vec4 baseColor;
} ubo;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-ubo.lightDir.xyz);          // light coming *toward* the surface
    float diff = max(dot(N, L), 0.0);

    vec3 ambient = 0.15 * ubo.baseColor.rgb;
    vec3 lit = ambient + diff * ubo.baseColor.rgb;

    outColor = vec4(lit, 1.0);
}
