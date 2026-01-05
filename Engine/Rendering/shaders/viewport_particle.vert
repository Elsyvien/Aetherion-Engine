#version 450

// Particle billboard vertex shader
// Generates camera-facing quads from particle instance data

layout(location = 0) in vec3 aPos;      // Quad vertex position (-0.5 to 0.5)
layout(location = 1) in vec2 aUv;       // Quad UV

// Per-particle instance data
layout(location = 2) in vec3 iPosition; // World position
layout(location = 3) in float iSize;    // Particle size
layout(location = 4) in vec4 iColor;    // Particle color RGBA
layout(location = 5) in float iRotation; // Rotation in radians

const uint kMaxLights = 8u;
const uint kShadowCascadeCount = 4u;

struct LightUniform {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 spot;
};

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 uViewProj;
    vec4 uLightDir;
    vec4 uLightColor;
    vec4 uAmbientColor;
    vec4 uCameraPos;
    vec4 uFrameParams;
    vec4 uMaterialParams;
    vec4 uLightCounts;
    LightUniform uLights[kMaxLights];
    mat4 uShadowMatrices[kShadowCascadeCount];
    vec4 uShadowSplits;
    vec4 uShadowParams;
    vec4 uPostParams;
    vec4 uFrustumPlanes[6];
} ubo;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;

void main() {
    // Billboard: always face the camera
    vec3 cameraRight = vec3(
        ubo.uViewProj[0][0],
        ubo.uViewProj[1][0],
        ubo.uViewProj[2][0]
    );
    vec3 cameraUp = vec3(
        ubo.uViewProj[0][1],
        ubo.uViewProj[1][1],
        ubo.uViewProj[2][1]
    );
    
    // Apply rotation
    float cosR = cos(iRotation);
    float sinR = sin(iRotation);
    vec2 rotatedPos = vec2(
        aPos.x * cosR - aPos.y * sinR,
        aPos.x * sinR + aPos.y * cosR
    );
    
    // Calculate world position
    vec3 worldPos = iPosition 
        + cameraRight * rotatedPos.x * iSize 
        + cameraUp * rotatedPos.y * iSize;
    
    gl_Position = ubo.uViewProj * vec4(worldPos, 1.0);
    
    vUv = aUv;
    vColor = iColor;
}
