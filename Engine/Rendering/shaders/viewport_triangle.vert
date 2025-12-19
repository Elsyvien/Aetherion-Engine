#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 uViewProj;
} ubo;

layout(push_constant) uniform InstancePC
{
    mat4 uModel;
    vec4 uColor;
} pc;

layout(location = 0) out vec3 vColor;

void main()
{
    gl_Position = ubo.uViewProj * pc.uModel * vec4(aPos, 0.0, 1.0);
    vColor = aColor * pc.uColor.rgb;
}
