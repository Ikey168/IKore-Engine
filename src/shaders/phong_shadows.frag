#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace; // Position in light space for shadow mapping

struct Material {
    // Support for both texture-based and color-based materials
    sampler2D diffuse;
    sampler2D specular;
    sampler2D normal;
    vec3 ambientColor;
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
    
    // Flags to determine whether to use textures or colors
    bool useDiffuseTexture;
    bool useSpecularTexture;
    bool useNormalTexture;
};

struct DirectionalLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    
    // Shadow mapping
    bool castShadows;
    sampler2D shadowMap;
    mat4 lightSpaceMatrix;
    float shadowBias;
    bool softShadows;
    int pcfKernelSize;
};

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    
    float constant;
    float linear;
    float quadratic;
    
    // Shadow mapping
    bool castShadows;
    samplerCube shadowCubemap;
    float farPlane;
    float shadowBias;
    bool softShadows;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    
    float constant;
    float linear;
    float quadratic;
    
    float cutOff;
    float outerCutOff;
    
    // Shadow mapping (similar to directional light)
    bool castShadows;
    sampler2D shadowMap;
    mat4 lightSpaceMatrix;
    float shadowBias;
    bool softShadows;
    int pcfKernelSize;
};

#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4

uniform Material material;
uniform DirectionalLight dirLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];
uniform int numPointLights;
uniform int numSpotLights;
uniform bool useDirLight;

uniform vec3 viewPos;

// Shadow mapping functions
float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap, float bias, bool softShadows, int kernelSize);
float PointShadowCalculation(vec3 fragPos, vec3 lightPos, samplerCube shadowCubemap, float farPlane, float bias, bool softShadows);

// Lighting calculation functions
vec3 CalcDirLight(DirectionalLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    vec3 result = vec3(0.0);
    
    // Directional light
    if (useDirLight) {
        result += CalcDirLight(dirLight, norm, viewDir);
    }
    
    // Point lights
    for (int i = 0; i < numPointLights && i < MAX_POINT_LIGHTS; i++) {
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir);
    }
    
    // Spot lights
    for (int i = 0; i < numSpotLights && i < MAX_SPOT_LIGHTS; i++) {
        result += CalcSpotLight(spotLights[i], norm, FragPos, viewDir);
    }
    
    FragColor = vec4(result, 1.0);
}

vec3 CalcDirLight(DirectionalLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Get material properties (texture or color)
    vec3 diffuseColor = material.useDiffuseTexture ? 
                       texture(material.diffuse, TexCoord).rgb : material.diffuseColor;
    vec3 specularColor = material.useSpecularTexture ? 
                        texture(material.specular, TexCoord).rgb : material.specularColor;
    
    // Combine results
    vec3 ambient = light.ambient * material.ambientColor * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;
    
    // Calculate shadow
    float shadow = 0.0;
    if (light.castShadows) {
        shadow = ShadowCalculation(FragPosLightSpace, light.shadowMap, light.shadowBias, 
                                 light.softShadows, light.pcfKernelSize);
    }
    
    // Apply shadow to diffuse and specular components (not ambient)
    return ambient + (1.0 - shadow) * (diffuse + specular);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    // Get material properties (texture or color)
    vec3 diffuseColor = material.useDiffuseTexture ? 
                       texture(material.diffuse, TexCoord).rgb : material.diffuseColor;
    vec3 specularColor = material.useSpecularTexture ? 
                        texture(material.specular, TexCoord).rgb : material.specularColor;
    
    // Combine results
    vec3 ambient = light.ambient * material.ambientColor * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;
    
    // Calculate shadow
    float shadow = 0.0;
    if (light.castShadows) {
        shadow = PointShadowCalculation(fragPos, light.position, light.shadowCubemap, 
                                      light.farPlane, light.shadowBias, light.softShadows);
    }
    
    // Apply attenuation and shadow
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    
    return ambient + (1.0 - shadow) * (diffuse + specular);
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    // Spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    // Get material properties (texture or color)
    vec3 diffuseColor = material.useDiffuseTexture ? 
                       texture(material.diffuse, TexCoord).rgb : material.diffuseColor;
    vec3 specularColor = material.useSpecularTexture ? 
                        texture(material.specular, TexCoord).rgb : material.specularColor;
    
    // Combine results
    vec3 ambient = light.ambient * material.ambientColor * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;
    
    // Calculate shadow (if the light is within the spotlight cone)
    float shadow = 0.0;
    if (light.castShadows && intensity > 0.0) {
        vec4 fragPosLightSpace = light.lightSpaceMatrix * vec4(fragPos, 1.0);
        shadow = ShadowCalculation(fragPosLightSpace, light.shadowMap, light.shadowBias, 
                                 light.softShadows, light.pcfKernelSize);
    }
    
    // Apply attenuation, intensity, and shadow
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    
    return ambient + (1.0 - shadow) * (diffuse + specular);
}

// Shadow mapping calculation for directional and spot lights
float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap, float bias, bool softShadows, int kernelSize) {
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    
    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    
    // Check if current fragment is in shadow
    if (projCoords.z > 1.0) {
        return 0.0; // Outside far plane, not in shadow
    }
    
    if (!softShadows || kernelSize <= 1) {
        // Hard shadows
        return currentDepth - bias > closestDepth ? 1.0 : 0.0;
    } else {
        // Soft shadows using PCF (Percentage Closer Filtering)
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        int halfKernel = kernelSize / 2;
        
        for(int x = -halfKernel; x <= halfKernel; ++x) {
            for(int y = -halfKernel; y <= halfKernel; ++y) {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
            }    
        }
        shadow /= float(kernelSize * kernelSize);
        
        return shadow;
    }
}

// Shadow mapping calculation for point lights (omnidirectional)
float PointShadowCalculation(vec3 fragPos, vec3 lightPos, samplerCube shadowCubemap, float farPlane, float bias, bool softShadows) {
    // Get vector between fragment position and light position
    vec3 fragToLight = fragPos - lightPos;
    
    // Get current linear depth as the length between the fragment and light position
    float currentDepth = length(fragToLight);
    
    if (!softShadows) {
        // Hard shadows
        float closestDepth = texture(shadowCubemap, fragToLight).r;
        closestDepth *= farPlane; // Undo mapping [0;1]
        
        return currentDepth - bias > closestDepth ? 1.0 : 0.0;
    } else {
        // Soft shadows using PCF for cubemap
        float shadow = 0.0;
        float samples = 20.0;
        float offset = 0.1;
        
        for(float x = -offset; x < offset; x += offset / (samples * 0.5)) {
            for(float y = -offset; y < offset; y += offset / (samples * 0.5)) {
                for(float z = -offset; z < offset; z += offset / (samples * 0.5)) {
                    float closestDepth = texture(shadowCubemap, fragToLight + vec3(x, y, z)).r;
                    closestDepth *= farPlane; // Undo mapping [0;1]
                    if(currentDepth - bias > closestDepth)
                        shadow += 1.0;
                }
            }
        }
        shadow /= (samples * samples * samples);
        
        return shadow;
    }
}
