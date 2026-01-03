#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec3 vWorldPos;
layout(location = 0) out vec4 outColor;

const uint kMaxLights = 8u;

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
} ubo;

layout(set = 1, binding = 0) uniform sampler2D uAlbedoMap;
layout(set = 1, binding = 1) uniform sampler2D uNormalMap;
layout(set = 1, binding = 2) uniform sampler2D uMetallicRoughnessMap;
layout(set = 1, binding = 3) uniform sampler2D uEmissiveMap;
layout(set = 1, binding = 4) uniform sampler2D uOcclusionMap;
layout(set = 1, binding = 5) uniform MaterialUBO {
    vec4 baseColor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
} material;

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

float DistanceAttenuation(float distance, float range)
{
    float invDist = 1.0 / max(distance * distance, 1.0);
    if (range <= 0.0)
    {
        return invDist;
    }
    float ratio = distance / max(range, 0.001);
    float falloff = clamp(1.0 - ratio, 0.0, 1.0);
    return invDist * falloff * falloff;
}

vec3 ApplyLight(vec3 l,
                vec3 radiance,
                float attenuation,
                vec3 n,
                vec3 v,
                vec3 albedo,
                vec3 f0,
                float metallic,
                float roughness,
                float nDotV)
{
    float nDotL = max(dot(n, l), 0.0);
    if (nDotL <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 h = normalize(v + l);
    float hDotV = max(dot(h, v), 0.0);

    vec3 f = FresnelSchlick(hDotV, f0);
    float d = DistributionGGX(n, h, roughness);
    float g = GeometrySmith(n, v, l, roughness);

    vec3 numerator = d * g * f;
    float denom = max(4.0 * nDotV * nDotL, 0.0001);
    vec3 specular = numerator / denom;

    vec3 kS = f;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / kPi;

    return (diffuse + specular) * radiance * nDotL * attenuation;
}

void main()
{
    vec4 baseColor = texture(uAlbedoMap, vUv) * material.baseColor * vec4(vColor, 1.0);
    vec3 albedo = baseColor.rgb;
    
    // Normal Mapping
    vec3 normalMap = texture(uNormalMap, vUv).rgb;
    vec3 N = normalize(vNormal);
    // Simple tangent reconstruction if tangents aren't passed
    vec3 Q1  = dFdx(vWorldPos);
    vec3 Q2  = dFdy(vWorldPos);
    vec2 st1 = dFdx(vUv);
    vec2 st2 = dFdy(vUv);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    
    // If normal map is present (default blue), use it. 
    // We assume default texture is (0.5, 0.5, 1.0) for normals.
    vec3 tangentNormal = normalMap * 2.0 - 1.0;
    vec3 n = normalize(TBN * tangentNormal);

    // MRAO
    vec4 mrSample = texture(uMetallicRoughnessMap, vUv);
    float metallicSample = mrSample.b;
    float roughnessSample = mrSample.g;
    float metallic = clamp(metallicSample * material.metallicFactor, 0.0, 1.0);
    float roughness = clamp(roughnessSample * material.roughnessFactor, 0.04, 1.0);
    
    float occlusion = texture(uOcclusionMap, vUv).r;
    vec3 emissive = texture(uEmissiveMap, vUv).rgb * material.emissiveFactor.rgb;

    if ((pc.uFlags & 1u) != 0u)
    {
        outColor = vec4(albedo, baseColor.a);
        return;
    }

    uint debugMode = uint(ubo.uFrameParams.x + 0.5);
    if (debugMode == kDebugNormals)
    {
        outColor = vec4(n * 0.5 + 0.5, 1.0);
        return;
    }
    if (debugMode == kDebugRoughness)
    {
        outColor = vec4(vec3(roughness), 1.0);
        return;
    }
    if (debugMode == kDebugMetallic)
    {
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

    vec3 v = normalize(ubo.uCameraPos.xyz - vWorldPos);

    float nDotV = max(dot(n, v), 0.0);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    vec3 lighting = vec3(0.0);

    int dirCount = int(ubo.uLightCounts.x + 0.5);
    int pointCount = int(ubo.uLightCounts.y + 0.5);
    int spotCount = int(ubo.uLightCounts.z + 0.5);
    int totalCount = int(ubo.uLightCounts.w + 0.5);

    if (totalCount <= 0)
    {
        vec3 l = normalize(-ubo.uLightDir.xyz);
        lighting += ApplyLight(l,
                               ubo.uLightColor.rgb,
                               1.0,
                               n,
                               v,
                               albedo,
                               f0,
                               metallic,
                               roughness,
                               nDotV);
    }
    else
    {
        int index = 0;
        for (int i = 0; i < dirCount; ++i)
        {
            LightUniform light = ubo.uLights[index++];
            vec3 l = normalize(-light.direction.xyz);
            lighting += ApplyLight(l,
                                   light.color.rgb,
                                   1.0,
                                   n,
                                   v,
                                   albedo,
                                   f0,
                                   metallic,
                                   roughness,
                                   nDotV);
        }

        for (int i = 0; i < pointCount; ++i)
        {
            LightUniform light = ubo.uLights[index++];
            vec3 toLight = light.position.xyz - vWorldPos;
            float dist = length(toLight);
            vec3 l = (dist > 0.0001) ? (toLight / dist) : vec3(0.0, 1.0, 0.0);
            float attenuation = DistanceAttenuation(dist, light.position.w);
            lighting += ApplyLight(l,
                                   light.color.rgb,
                                   attenuation,
                                   n,
                                   v,
                                   albedo,
                                   f0,
                                   metallic,
                                   roughness,
                                   nDotV);
        }

        for (int i = 0; i < spotCount; ++i)
        {
            LightUniform light = ubo.uLights[index++];
            vec3 toLight = light.position.xyz - vWorldPos;
            float dist = length(toLight);
            vec3 l = (dist > 0.0001) ? (toLight / dist) : vec3(0.0, 1.0, 0.0);
            float attenuation = DistanceAttenuation(dist, light.position.w);

            float cosTheta = dot(normalize(-l), normalize(light.direction.xyz));
            float innerCos = light.spot.x;
            float outerCos = light.spot.y;
            float denom = max(innerCos - outerCos, 0.0001);
            float spotFactor = clamp((cosTheta - outerCos) / denom, 0.0, 1.0);

            lighting += ApplyLight(l,
                                   light.color.rgb,
                                   attenuation * spotFactor,
                                   n,
                                   v,
                                   albedo,
                                   f0,
                                   metallic,
                                   roughness,
                                   nDotV);
        }
    }

    vec3 ambient = ubo.uAmbientColor.rgb * albedo * occlusion;
    outColor = vec4(ambient + lighting + emissive, baseColor.a);
}
