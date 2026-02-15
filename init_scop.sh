#!/bin/bash
set -e

mkdir -p include/scop src shaders assets/models assets/textures

cat > src/main.cpp <<'CPP'
#include "scop/VulkanApp.hpp"
#include <iostream>

int main() {
    try {
        scop::VulkanApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
CPP

cat > include/scop/VulkanApp.hpp <<'HPP'
#pragma once
namespace scop {
class VulkanApp {
public:
    void run();
};
}
HPP

cat > src/VulkanApp.cpp <<'CPP'
#include "scop/VulkanApp.hpp"
#include <stdexcept>

namespace scop {
void VulkanApp::run() {
    // TODO: initWindow (GLFW)
    // TODO: initVulkan
    // TODO: mainLoop
    // TODO: cleanup
    throw std::runtime_error("VulkanApp::run not implemented yet");
}
}
CPP

cat > shaders/mesh.vert.glsl <<'GLSL'
#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UBO {
    mat4 mvp;
    float mixFactor; // 0=color, 1=texture
} ubo;

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUV;
layout(location = 2) out float vMix;

void main() {
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
    vColor = inColor;
    vUV = inUV;
    vMix = ubo.mixFactor;
}
GLSL

cat > shaders/mesh.frag.glsl <<'GLSL'
#version 450
layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in float vMix;

layout(binding = 1) uniform sampler2D tex;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 t = texture(tex, vUV).rgb;
    vec3 c = mix(vColor, t, clamp(vMix, 0.0, 1.0)); // smooth transition
    outColor = vec4(c, 1.0);
}
GLSL

chmod +x init_scop.sh 2>/dev/null || true
echo "✅ SCOP skeleton created."
echo "Next: implement Makefile + Vulkan/GLFW init and compile shaders to SPV."
