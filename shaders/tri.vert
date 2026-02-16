#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNrm;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 vPos;   // world position
layout(location = 1) out vec3 vNrm;   // world normal
layout(location = 2) out vec2 vUV;    // uv

layout(set = 0, binding = 0, std140) uniform UBO
{
    mat4 vp;
    mat4 model;
    vec4 lightDir;
    vec4 baseColor;
    vec4 cameraPos;
    vec4 spec;
    vec4 texMix;
} ubo;

void main()
{
    vec4 world = ubo.model * vec4(inPos, 1.0);
    gl_Position = ubo.vp * world;

    vPos = world.xyz;

    // Good enough unless you do heavy non-uniform scaling.
    vNrm = normalize(mat3(ubo.model) * inNrm);

    vUV = inUV;
}
