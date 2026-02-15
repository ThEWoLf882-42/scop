#version 450

layout(location=0) in vec3 vNrm;
layout(location=1) in vec2 vUV;

layout(set=0,binding=0) uniform UBO {
    mat4 vp;
    mat4 model;
    vec4 lightDir;
    vec4 baseColor;
    vec4 cameraPos;
    vec4 spec;
} ubo;

layout(set=0,binding=1) uniform sampler2D uTex;

layout(location=0) out vec4 outColor;

void main() {
    vec3 N = normalize(vNrm);
    vec3 L = normalize(-ubo.lightDir.xyz);

    float diff = max(dot(N, L), 0.0);

    vec3 albedo = texture(uTex, vUV).rgb;
    vec3 col = albedo * (0.20 + 0.80 * diff);

    outColor = vec4(col, 1.0);
}
