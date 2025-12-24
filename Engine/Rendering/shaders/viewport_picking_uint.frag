#version 450

layout(location = 0) out uint outId;

layout(push_constant) uniform InstancePC
{
    mat4 uModel;
    vec4 uColor;
    uint uEntityId;
    uint uFlags;
    vec2 uPad;
} pc;

void main()
{
    outId = pc.uEntityId;
}
