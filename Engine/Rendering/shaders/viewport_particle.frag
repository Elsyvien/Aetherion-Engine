#version 450

// Particle fragment shader
// Renders soft circular particles with color/alpha from vertex shader

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

// Optional texture sampler (binding 1 if available)
layout(set = 1, binding = 0) uniform sampler2D uParticleTexture;

layout(push_constant) uniform ParticlePC {
    uint uUseTexture; // 0 = procedural circle, 1 = texture
    uint uPad[3];
} pc;

void main() {
    vec4 texColor;
    
    if (pc.uUseTexture == 1u) {
        // Sample particle texture
        texColor = texture(uParticleTexture, vUv);
    } else {
        // Procedural soft circle
        vec2 center = vUv - vec2(0.5);
        float dist = length(center) * 2.0;
        
        // Soft edge falloff
        float alpha = 1.0 - smoothstep(0.7, 1.0, dist);
        
        // Discard pixels outside circle
        if (alpha < 0.01) {
            discard;
        }
        
        texColor = vec4(1.0, 1.0, 1.0, alpha);
    }
    
    // Apply particle color
    fragColor = texColor * vColor;
    
    // Premultiplied alpha for additive blending compatibility
    fragColor.rgb *= fragColor.a;
}
