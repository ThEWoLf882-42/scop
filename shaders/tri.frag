#version 450

layout(location = 0) in vec3 vPos;   // world position
layout(location = 1) in vec3 vNrm;   // world normal
layout(location = 2) in vec2 vUV;    // uv

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0, std140) uniform UBO
{
    mat4 vp;
    mat4 model;
    vec4 lightDir;    // xyz = light direction (surface -> light)
    vec4 baseColor;   // rgba
    vec4 cameraPos;   // xyz = camera position in world
    vec4 spec;        // x=shininess, y=specStrength, z=ambientStrength, w=unused
    vec4 texMix;      // x = mix factor (0 flat, 1 textured)
} ubo;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main()
{
    vec3 N = normalize(vNrm);
    vec3 L = normalize(ubo.lightDir.xyz);
    vec3 V = normalize(ubo.cameraPos.xyz - vPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);

    float shininess = max(1.0, ubo.spec.x);
    float specStrength = max(0.0, ubo.spec.y);
    float ambientStrength = clamp(ubo.spec.z, 0.0, 1.0);

    float specTerm = 0.0;
    if (diff > 0.0)
        specTerm = pow(max(dot(N, H), 0.0), shininess);

    vec4 tex = texture(texSampler, vUV);

    float m = clamp(ubo.texMix.x, 0.0, 1.0);

    vec3 albedoFlat = ubo.baseColor.rgb;
    vec3 albedoTex  = albedoFlat * tex.rgb;
    vec3 albedo     = mix(albedoFlat, albedoTex, m);

    float alpha = mix(ubo.baseColor.a, ubo.baseColor.a * tex.a, m);

    vec3 color = ambientStrength * albedo
               + diff * albedo
               + specStrength * specTerm * vec3(1.0);

    outColor = vec4(color, alpha);
}
