#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 params; // x = blend, y = hasRealTexture, z = hasMaterial
    vec4 kd;
    vec4 ksNs;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(0.45, 0.85, 0.35));
    vec3 V = normalize(vec3(0.0, 0.0, 1.0));
    vec3 R = reflect(-L, N);

    float diffuse = max(dot(N, L), 0.0);
    float specular = pow(max(dot(R, V), 0.0), max(ubo.ksNs.w, 1.0));

    vec4 whiteColor = vec4(1.0, 1.0, 1.0, 1.0);
    vec4 materialColor = vec4(ubo.kd.rgb, 1.0);
    vec4 texColor = texture(texSampler, fragUV);

    float blend = clamp(ubo.params.x, 0.0, 1.0);
    float hasRealTexture = ubo.params.y;
    float hasMaterial = ubo.params.z;

    vec4 targetColor = whiteColor;
    if (hasRealTexture > 0.5) {
        targetColor = texColor;
    } else if (hasMaterial > 0.5) {
        targetColor = materialColor;
    }

    vec4 baseColor = mix(whiteColor, targetColor, blend);

    float shade = 0.55 + 0.45 * diffuse;
    vec3 spec = ubo.ksNs.rgb * specular * 0.18;

    outColor = vec4(baseColor.rgb * shade + spec, 1.0);
}