#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec3 vWorldPos;
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

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;

layout(push_constant) uniform InstancePC
{
    mat4 uModel;
    vec4 uColor;
    uint uEntityId;
    uint uFlags;
    vec2 uPad;
} pc;

const float kPi = 3.14159265359;
const uint kDebugFinal = 0u;
const uint kDebugNormals = 1u;
const uint kDebugRoughness = 2u;
const uint kDebugMetallic = 3u;
const uint kDebugAlbedo = 4u;
const uint kDebugDepth = 5u;

float DistributionGGX(vec3 n, vec3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(n, h), 0.0);
    float denom = (nDotH * nDotH) * (a2 - 1.0) + 1.0;
    return a2 / max(kPi * denom * denom, 0.00001);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.00001);
}

float GeometrySmith(vec3 n, vec3 v, vec3 l, float roughness)
{
    float nDotV = max(dot(n, v), 0.0);
    float nDotL = max(dot(n, l), 0.0);
    float ggxV = GeometrySchlickGGX(nDotV, roughness);
    float ggxL = GeometrySchlickGGX(nDotL, roughness);
    return ggxV * ggxL;
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3 albedo = texture(uAlbedo, vUv).rgb * vColor;
    if ((pc.uFlags & 1u) != 0u)
    {
        outColor = vec4(albedo, 1.0);
        return;
    }

    uint debugMode = uint(ubo.uFrameParams.x + 0.5);
    if (debugMode == kDebugNormals)
    {
        vec3 n = normalize(vNormal);
        outColor = vec4(n * 0.5 + 0.5, 1.0);
        return;
    }
    if (debugMode == kDebugRoughness)
    {
        float roughness = clamp(ubo.uMaterialParams.y, 0.0, 1.0);
        outColor = vec4(vec3(roughness), 1.0);
        return;
    }
    if (debugMode == kDebugMetallic)
    {
        float metallic = clamp(ubo.uMaterialParams.x, 0.0, 1.0);
        outColor = vec4(vec3(metallic), 1.0);
        return;
    }
    if (debugMode == kDebugAlbedo)
    {
        outColor = vec4(albedo, 1.0);
        return;
    }
    if (debugMode == kDebugDepth)
    {
        float nearPlane = max(ubo.uFrameParams.z, 0.0001);
        float farPlane = max(ubo.uFrameParams.w, nearPlane + 0.0001);
        float depth = gl_FragCoord.z;
        float linearDepth = (nearPlane * farPlane) / max(farPlane - depth * (farPlane - nearPlane), 0.00001);
        float depth01 = clamp((linearDepth - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);
        outColor = vec4(vec3(depth01), 1.0);
        return;
    }

    vec3 n = normalize(vNormal);
    vec3 v = normalize(ubo.uCameraPos.xyz - vWorldPos);
    vec3 l = normalize(-ubo.uLightDir.xyz);
    vec3 h = normalize(v + l);

    float metallic = clamp(ubo.uMaterialParams.x, 0.0, 1.0);
    float roughness = clamp(ubo.uMaterialParams.y, 0.04, 1.0);

    float nDotL = max(dot(n, l), 0.0);
    float nDotV = max(dot(n, v), 0.0);
    float hDotV = max(dot(h, v), 0.0);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 f = FresnelSchlick(hDotV, f0);
    float d = DistributionGGX(n, h, roughness);
    float g = GeometrySmith(n, v, l, roughness);

    vec3 numerator = d * g * f;
    float denom = max(4.0 * nDotV * nDotL, 0.0001);
    vec3 specular = numerator / denom;

    vec3 kS = f;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / kPi;

    vec3 radiance = ubo.uLightColor.rgb;
    vec3 color = (diffuse + specular) * radiance * nDotL;
    vec3 ambient = ubo.uAmbientColor.rgb * albedo;

    outColor = vec4(ambient + color, 1.0);
}
