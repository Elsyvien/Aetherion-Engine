#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

const uint kMaxLights = 8u;
const uint kShadowCascadeCount = 4u;

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
    mat4 uShadowMatrices[kShadowCascadeCount];
    vec4 uShadowSplits;
    vec4 uShadowParams;
    vec4 uPostParams;
    vec4 uFrustumPlanes[6];
} ubo;

layout(set = 1, binding = 0) uniform sampler2D uScene;
layout(set = 1, binding = 1) uniform sampler2D uPicking;
layout(set = 1, binding = 2) uniform sampler2D uHistory;

const uint kDebugFinal = 0u;
const uint kDebugEntityId = 6u;

vec3 ToneMapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e),
                 0.0, 1.0);
}

uint DecodeEntityId(vec4 packed)
{
    uvec4 bytes = uvec4(round(clamp(packed, 0.0, 1.0) * 255.0));
    return bytes.r | (bytes.g << 8) | (bytes.b << 16) | (bytes.a << 24);
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

vec3 SampleBloom(vec2 uv, float threshold)
{
    vec2 texel = 1.0 / vec2(textureSize(uScene, 0));
    vec3 bloom = vec3(0.0);

    vec3 center = max(texture(uScene, uv).rgb - vec3(threshold), vec3(0.0));
    bloom += center * 0.5;
    bloom += max(texture(uScene, uv + vec2(texel.x, 0.0)).rgb -
                     vec3(threshold),
                 vec3(0.0)) *
             0.125;
    bloom += max(texture(uScene, uv - vec2(texel.x, 0.0)).rgb -
                     vec3(threshold),
                 vec3(0.0)) *
             0.125;
    bloom += max(texture(uScene, uv + vec2(0.0, texel.y)).rgb -
                     vec3(threshold),
                 vec3(0.0)) *
             0.125;
    bloom += max(texture(uScene, uv - vec2(0.0, texel.y)).rgb -
                     vec3(threshold),
                 vec3(0.0)) *
             0.125;
    return bloom;
}

void main()
{
    vec2 uv = clamp(vUv, 0.0, 1.0);
    uint debugMode = uint(ubo.uFrameParams.x + 0.5);
    if (debugMode == kDebugEntityId)
    {
        uint id = DecodeEntityId(texture(uPicking, uv));
        outColor = vec4(FalseColor(id), 1.0);
        return;
    }

    vec3 color = texture(uScene, uv).rgb;
    if (debugMode == kDebugFinal)
    {
        float bloomThreshold = max(ubo.uPostParams.x, 0.0);
        float bloomIntensity = max(ubo.uPostParams.y, 0.0);
        if (bloomIntensity > 0.0001)
        {
            vec3 bloom = SampleBloom(uv, bloomThreshold);
            color += bloom * bloomIntensity;
        }

        float exposure = max(ubo.uFrameParams.y, 0.0001);
        color = ToneMapACES(color * exposure);
    }

    if (ubo.uMaterialParams.z < 0.5)
    {
        color = pow(color, vec3(1.0 / 2.2));
    }

    if (debugMode == kDebugFinal && ubo.uPostParams.w > 0.5)
    {
        float blend = clamp(ubo.uPostParams.z, 0.0, 1.0);
        vec3 history = texture(uHistory, uv).rgb;
        color = mix(color, history, blend);
    }

    outColor = vec4(color, 1.0);
}
