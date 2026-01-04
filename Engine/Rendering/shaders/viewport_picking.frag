#version 450

layout(location = 4) flat in uint vEntityId;
layout(location = 0) out vec4 outColor;

void main()
{
    uint id = vEntityId;
    outColor = vec4(float(id & 0xFFu) / 255.0,
                    float((id >> 8) & 0xFFu) / 255.0,
                    float((id >> 16) & 0xFFu) / 255.0,
                    float((id >> 24) & 0xFFu) / 255.0);
}
