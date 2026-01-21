#version 450

layout(location = 2) in vec2 vUv;
layout(location = 4) flat in uint vEntityId;
layout(location = 0) out uint outId;

layout(set = 1, binding = 0) uniform sampler2D uAlbedoMap;
layout(set = 1, binding = 5) uniform MaterialUBO {
    vec4 baseColor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float alphaMode;
} material;

const float kAlphaModeMask = 1.0;

void main()
{
    float alpha = texture(uAlbedoMap, vUv).a * material.baseColor.a;
    if (material.alphaMode > (kAlphaModeMask - 0.5) &&
        material.alphaMode < (kAlphaModeMask + 0.5) &&
        alpha < material.alphaCutoff)
    {
        discard;
    }

    outId = vEntityId;
}
