#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aUv;

const uint kMaxLights = 8u;

struct LightUniform
{
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 spot;
};

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 uViewProj;
    vec4 uLightDir;
    vec4 uLightColor;
    vec4 uAmbientColor;
    vec4 uCameraPos;
    vec4 uFrameParams;
    vec4 uMaterialParams;
    vec4 uLightCounts;
    LightUniform uLights[kMaxLights];
} ubo;

layout(push_constant) uniform InstancePC
{
    mat4 uModel;
    vec4 uColor;
    uint uEntityId;
    uint uFlags;
    vec2 uPad;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vColor;
layout(location = 2) out vec2 vUv;
layout(location = 3) out vec3 vWorldPos;

void main()
{
    vec4 worldPos = pc.uModel * vec4(aPos, 1.0);
    gl_Position = ubo.uViewProj * worldPos;
    mat3 normalMat = mat3(transpose(inverse(pc.uModel)));
    vNormal = normalize(normalMat * aNormal);
    vColor = aColor.rgb * pc.uColor.rgb;
    vUv = aUv;
    vWorldPos = worldPos.xyz;
}
