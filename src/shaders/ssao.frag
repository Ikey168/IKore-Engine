#version 330 core
out float FragColor;

in vec2 TexCoord;

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 view;

uniform float radius;
uniform float bias;
uniform int sampleCount;

// Tile noise texture over screen based on screen dimensions divided by noise size
const vec2 noiseScale = vec2(1280.0/4.0, 720.0/4.0); // screen = 1280x720

void main() {
    // Get input for SSAO algorithm
    float depth = texture(gDepth, TexCoord).x;
    vec3 normal = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);
    vec3 randomVec = normalize(texture(texNoise, TexCoord * noiseScale).xyz);
    
    // Reconstruct position from depth
    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpacePosition = inverse(projection) * clipSpacePosition;
    vec3 fragPos = viewSpacePosition.xyz / viewSpacePosition.w;
    
    // Create TBN matrix: Gram-Schmidt process
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    // Iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    for(int i = 0; i < sampleCount; ++i) {
        // Get sample position
        vec3 samplePos = TBN * samples[i]; // from tangent to view-space
        samplePos = fragPos + samplePos * radius; 
        
        // Project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // Get sample depth
        float sampleDepth = texture(gDepth, offset.xy).x;
        
        // Reconstruct sample depth in view space
        vec4 sampleClipPos = vec4(offset.xy * 2.0 - 1.0, sampleDepth * 2.0 - 1.0, 1.0);
        vec4 sampleViewPos = inverse(projection) * sampleClipPos;
        float sampleViewDepth = (sampleViewPos.z / sampleViewPos.w);
        
        // Range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleViewDepth));
        occlusion += (sampleViewDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(sampleCount));
    
    FragColor = occlusion;
}
