#version 330 core
out vec4 FragColor;

in vec3 LocalPos;

uniform samplerCube environmentMap;

const float PI = 3.14159265359;

// Diffuse irradiance convolution (issue #270): cosine-weighted hemisphere integral of
// the environment around the surface normal, producing the diffuse IBL term. Rendered
// once per face into a small irradiance cubemap at load. The cos(theta)*sin(theta)
// weighting is the Lambertian * solid-angle factor.
void main() {
    vec3 N = normalize(LocalPos);

    vec3 irradiance = vec3(0.0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    const float sampleDelta = 0.025;
    float nrSamples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // Spherical -> cartesian in tangent space, then to world.
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / max(nrSamples, 1.0));

    FragColor = vec4(irradiance, 1.0);
}
