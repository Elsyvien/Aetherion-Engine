#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 uViewProj;
    vec4 uLightDir;
    vec4 uLightColor;
    vec4 uAmbientColor;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(-ubo.uLightDir.xyz);
    float ndotl = max(dot(normal, lightDir), 0.0);

    vec3 texColor = texture(uAlbedo, vUv).rgb;
    vec3 lit = texColor * vColor * (ubo.uAmbientColor.rgb + ubo.uLightColor.rgb * ndotl);
    if (ubo.uLightDir.w > 0.5)
    {
        lit = pow(lit, vec3(1.0 / 2.2));
    }

    outColor = vec4(lit, 1.0);
}
