#version 450

layout(location = 0) out vec4 outColor;

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
    uint id = pc.uEntityId;
    outColor = vec4(float(id & 0xFFu) / 255.0,
                    float((id >> 8) & 0xFFu) / 255.0,
                    float((id >> 16) & 0xFFu) / 255.0,
                    float((id >> 24) & 0xFFu) / 255.0);
}
