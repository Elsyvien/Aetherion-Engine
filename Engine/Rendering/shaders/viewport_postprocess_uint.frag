#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 uViewProj;
    vec4 uLightDir;
    vec4 uLightColor;
    vec4 uAmbientColor;
    vec4 uCameraPos;
    vec4 uFrameParams;
    vec4 uMaterialParams;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D uScene;
layout(set = 1, binding = 1) uniform usampler2D uPicking;

const uint kDebugFinal = 0u;
const uint kDebugEntityId = 6u;

vec3 ToneMapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 FalseColor(uint id)
{
    if (id == 0u)
    {
        return vec3(0.0);
    }
    uint r = (id * 1973u + 9277u) & 255u;
    uint g = (id * 9277u + 26699u) & 255u;
    uint b = (id * 26699u + 31883u) & 255u;
    return vec3(r, g, b) / 255.0;
}

void main()
{
    uint debugMode = uint(ubo.uFrameParams.x + 0.5);
    if (debugMode == kDebugEntityId)
    {
        ivec2 size = textureSize(uPicking, 0);
        vec2 uv = clamp(vUv, 0.0, 1.0);
        ivec2 coord = ivec2(uv * vec2(size));
        coord = clamp(coord, ivec2(0), size - ivec2(1));
        uint id = texelFetch(uPicking, coord, 0).r;
        outColor = vec4(FalseColor(id), 1.0);
        return;
    }

    vec3 color = texture(uScene, vUv).rgb;
    if (debugMode == kDebugFinal)
    {
        float exposure = max(ubo.uFrameParams.y, 0.0001);
        color = ToneMapACES(color * exposure);
    }

    if (ubo.uMaterialParams.z < 0.5)
    {
        color = pow(color, vec3(1.0 / 2.2));
    }

    outColor = vec4(color, 1.0);
}
