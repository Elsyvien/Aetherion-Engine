#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aUv;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 uViewProj;
    vec4 uLightDir;
    vec4 uLightColor;
    vec4 uAmbientColor;
} ubo;

layout(push_constant) uniform InstancePC
{
    mat4 uModel;
    vec4 uColor;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vColor;
layout(location = 2) out vec2 vUv;

void main()
{
    gl_Position = ubo.uViewProj * pc.uModel * vec4(aPos, 1.0);
    mat3 normalMat = mat3(transpose(inverse(pc.uModel)));
    vNormal = normalize(normalMat * aNormal);
    vColor = aColor.rgb * pc.uColor.rgb;
    vUv = aUv;
}
