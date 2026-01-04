#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aUv;
layout(location = 4) in vec4 iModel0;
layout(location = 5) in vec4 iModel1;
layout(location = 6) in vec4 iModel2;
layout(location = 7) in vec4 iModel3;
layout(location = 8) in vec4 iColor;
layout(location = 9) in uint iEntityId;
layout(location = 10) in uint iFlags;

const uint kInstanceFlagUseInstanceData = 2u;

layout(set = 0, binding = 0) uniform ShadowUBO
{
    mat4 uLightViewProj;
} ubo;

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
    mat4 model;
    if ((pc.uFlags & kInstanceFlagUseInstanceData) != 0u)
    {
        model = mat4(iModel0, iModel1, iModel2, iModel3);
    }
    else
    {
        model = pc.uModel;
    }

    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = ubo.uLightViewProj * worldPos;
}
