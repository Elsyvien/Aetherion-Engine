#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 uMVP;
} ubo;

layout(location = 0) out vec3 vColor;

void main()
{
    gl_Position = ubo.uMVP * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
