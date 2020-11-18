/*
TODO:
1) total refactor of the fragment shader to specialize in PBR materials
*/

#version 410 core

const float PI = 3.14159265359;

// output shader variable
out vec4 colorFrag;

// vector from fragment to camera
in vec3 tViewDirection; // in tangent coordinates (for illumination calculations)

// TBN matrix, from tangent space to world coordinates
in mat3 wTBNt;

// interpolated texture coordinates
in vec2 interp_UV;

// texture repetitions
uniform vec2 repeat;

// texture sampler
uniform sampler2D albedo;

// normal map sampler
uniform sampler2D normMap;
// depth map sampler
uniform sampler2D depthMap;
// ambient occlusion map sampler
uniform sampler2D aoMap;
// metalness map sampler
uniform sampler2D metallicMap;

// quaternion map sampler
uniform sampler2D quaternionMap;
// differential map sampler
uniform sampler2D rotationMap;

// (spectral) fresnel reflectance at normal incidence
uniform vec3 F0;

// uniform for Parallax Mapping
uniform float heightScale;

// the same cubemap used for the skybox
uniform samplerCube environmentMap;

// the convoluted environment map
uniform samplerCube irradianceMap;

// the number of samples in the integration
uniform uint sampleCount;

// the RGBA LUT for half-vector sampling
// alpha channel is the probability density function for that vector
// u parameter is the first random number, v parameter is the second
uniform sampler2D halfVector; //in tangent space coordinates

// the RG LUT for the Monte-Carlo integration of the BRDF (for fixed (alphaX, alphaY) directional roughness)
// red channel is size (versus F0), green channel is bias
// u parameter is NdotV, v parameter is TdotV
uniform sampler2D brdfLUT;

////////////////////////////////////////////////////////////////////

// subroutine uniform for the choice of the specular lighting component method
subroutine vec3 diffuse_model();
subroutine uniform diffuse_model Diffuse;

// subroutine uniform for the choice of the specular lighting component method
subroutine vec3 specular_model();
subroutine uniform specular_model Specular;

////////////////////////////////////////////////////////////////////

// subroutine uniform for the choice of method for remapping UV coordinates based on displacement effects (e. g. Parallax Occlusion Mapping)
// viewDir must be in tangent space coordinates
subroutine vec2 displacement(vec2 texCoords, vec3 viewDir);
subroutine uniform displacement Displacement;

////////////////////////////////////////////////////////////////////

// subroutine uniforms for the choice of bump-mapping method (extended to the whole tangent space)

subroutine vec3 normal_map(vec2 final_UV);
subroutine uniform normal_map Normal_Map;

subroutine vec3 tangent_map(vec2 final_UV);
subroutine uniform tangent_map Tangent_Map;

subroutine vec3 bitangent_map(vec2 final_UV);
subroutine uniform bitangent_map Bitangent_Map;

////////////////////////////////////////////////////////////////////

// low discrepancy sequence generation
float RadicalInverse_VdC(uint bits);
vec2 Hammersley(uint i, uint N);

////////////////////////////////////////////////////////////////////
// Normalized Lambertian Diffuse Component
subroutine(diffuse_model)
vec3 Lambert_Irradiance() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
    // we determine UVs based on wrapping (repeat) and displacement
    vec3 V = normalize(tViewDirection);
    vec2 disp_UV = Displacement(mod(interp_UV*repeat, 1.0), V);
    vec2 final_UV = mod(disp_UV, 1.0);

    vec3 N = Normal_Map(final_UV);

    vec3 irradiance = texture(irradianceMap, wTBNt * N).xyz; // N is in tangent coordinates, with or without perturbation (bump mapping)
    vec3 surfaceColor = texture(albedo, final_UV).xyz;

    // division by PI is done in the convolution resulting in the irradiance map
    return irradiance*surfaceColor;
}

// realtime environment lookup, preprocessed BRDF integral and half-vector
subroutine(specular_model)
vec3 Specular_Irradiance()
{    
    // we determine UVs based on wrapping (repeat) and displacement
    vec3 V = normalize(tViewDirection); // view vector in tangent space coordinates
    vec2 disp_UV = Displacement(mod(interp_UV*repeat, 1.0), V);
    vec2 final_UV = mod(disp_UV, 1.0);

    // 1): sample and integrate the environment map

    // tangent, bitangent and normal in tangent space coordinates: for calculations
    // NOTE: this is where bump mapping happens when enabled, so don't assume the vectors are axis-aligned
    vec3 N = Normal_Map(final_UV);
    vec3 T = Tangent_Map(final_UV);
    vec3 B = Bitangent_Map(final_UV);

    /*
    The original article by Epic Games introduces this approximation to preprocess the prefiltered map (not knowing in advance the value of V):
    vec3 R = N; // reflected vector
    vec3 V = R; // view vector

    Since we can't preprocess, we might as well use the actual value of V! (R is not needed in this instance)
    */

    float totalWeight = 0.0;
    vec3 convolutedColor = vec3(0.0);  
    for (uint i = 0u; i < sampleCount; i++)
    {
        // low discrepancy sequence on the unit square
        vec2 Xi = Hammersley(i, sampleCount);

        // H, V and L are all in tangent space

        // mapping the square to the upper hemisphere via texture lookup
        vec3 H = 2.0 * texture(halfVector, Xi).xyz - 1.0;
        // H should be perturbed along with the tangent space (the distribution is centered on the perturbed normal and rotated along the perturbed tangents)
        H = H.x * T + H.y * B + H.z * N; // this is a matrix multiplication. Notice it maps tangent space to tangent space (it is an endomorphism)

        // calculate L as the reflection of V against H
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            // map the light direction to world coordinates, usign the TBN matrix interpolated during rasterization
            vec3 wL = wTBNt * L;

            // look up of the environment map along the (world) light direction
            convolutedColor += texture(environmentMap, wL).rgb * NdotL; // NdotL is the weight of the Monte-Carlo integration

            /*
            IMPORTANT NOTE: to avoid excessive noise due to undersampling, it would be better to sample a miplevel of the environment map
            Here is the point where it should be done.

            Original code is as such:

                ///
                float D   = DistributionGGX(NdotH, roughness);
                float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001; 

                float resolution = 512.0; // resolution of source cubemap (per face)
                float saTexel  = 4.0 * PI / (6.0 * resolution * resolution);
                float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

                float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
                ///

            REMEMBER TO ENABLE TRILINEAR FILTERING FOR THE CUBE MAP IN THE APPLICATION:

                ///
                glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
                ///
            
            AND TO GENERATE MIPMAP !AFTER! CUBEMAP IS SET FROM EQUIRECTANGULAR

            --> Code would be changed to:

                convolutedColor += textureLod(environmentMap, wL, mipLevel).rgb + NdotL
            */


            totalWeight += NdotL;
        }
    }
    convolutedColor = convolutedColor / totalWeight;

    // 2): calculate the resulting specular component, using the preprocessed BRDF integral

    // the BRDF has rectangular symmetry along the tangent and bitangent
    // this means it is enough to calculate it assuming V is in the first quadrant of the tangent plane, as H is instead mapped to all of the quadrants (and only their relative position, regardless of simmetry, matters)
    // this improves memory management by a factor of 4 at the same resolution
    float uLUT = abs(dot(T, V));
    float vLUT = abs(dot(B, V));

    // look up size and bias coefficients from the BRDF LUT
    vec2 envBRDF  = texture( brdfLUT, vec2(uLUT, vLUT) ).rg;

    float NdotV = max(dot(V, N), 0.0); // avoid raising a negative base
    vec3 F = vec3(pow(1.0 - NdotV, 5.0));
    F *= (1.0 - F0);
    F += F0;

    // compose all of the contributions
    vec3 specular = convolutedColor * (F * envBRDF.x + envBRDF.y); // envBRDF.x is size, envBRDF.y is bias

    return specular;
}

subroutine(displacement)
vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{ 
    // number of depth layers
    const float minLayers = 8;
    const float maxLayers = 32;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * heightScale; 
    vec2 deltaTexCoords = P / numLayers;
  
    // get initial values
    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = texture(depthMap, currentTexCoords).r;
      
    while(currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = texture(depthMap, currentTexCoords).r;  
        // get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    // get texture coordinates before collision (reverse operations)
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(depthMap, prevTexCoords).r - currentLayerDepth + layerDepth;
 
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

subroutine(normal_map)
vec3 Off_N(vec2 final_UV)
{
    return vec3(0.0, 0.0, 1.0);
}

subroutine(normal_map)
vec3 NormalMapping(vec2 final_UV)
{
    return 2.0 * texture(normMap, final_UV).xyz - 1.0;
}

subroutine(normal_map)
vec3 RotationMap_N(vec2 final_UV)
{
    vec3 q = 2.0 * texture(quaternionMap, final_UV).xyz - 1.0;
    float a = q.x;
    float b = q.y;
    float c = q.z;
    // float d = 0.0;
    return vec3(-2.0*a*c, 2.0*a*b, a*a - b*b - c*c);
}

subroutine(tangent_map)
vec3 Off_T(vec2 final_UV)
{
    return vec3(1.0, 0.0, 0.0);
}

subroutine(tangent_map)
vec3 Diff_T(vec2 final_UV)
{
    vec2 V = 2.0 * texture(rotationMap, final_UV).xy - 1.0;
    return vec3(V.x, V.y, 0.0);
}

subroutine(tangent_map)
vec3 RotationMap_T(vec2 final_UV)
{
    vec3 q = 2.0 * texture(quaternionMap, final_UV).xyz - 1.0;
    float a = q.x;
    float b = q.y;
    float c = q.z;
    // float d = 0.0;
    return vec3(a*a + b*b - c*c, 2.0*b*c, 2.0*a*c);
}

subroutine(tangent_map)
vec3 DiffAndRotMap_T(vec2 final_UV)
{
    vec2 V = 2.0 * texture(rotationMap, final_UV).xy - 1.0;
    vec3 q = 2.0 * texture(quaternionMap, final_UV).xyz - 1.0;
    float a = q.x;
    float b = q.y;
    float c = q.z;
    // float d = 0.0;
    return V.x * vec3(a*a + b*b - c*c, 2.0*b*c, 2.0*a*c) + V.y * vec3(2.0*b*c, a*a - b*b + c*c, -2.0*a*b);
}

subroutine(bitangent_map)
vec3 Off_B(vec2 final_UV)
{
    return vec3(0.0, 1.0, 0.0);
}

subroutine(bitangent_map)
vec3 Diff_B(vec2 final_UV)
{
    vec2 V = 2.0 * texture(rotationMap, final_UV).xy - 1.0;
    return vec3(-V.y, V.x, 0.0);
}

subroutine(bitangent_map)
vec3 RotationMap_B(vec2 final_UV)
{
    vec3 q = 2.0 * texture(quaternionMap, final_UV).xyz - 1.0;
    float a = q.x;
    float b = q.y;
    float c = q.z;
    // float d = 0.0;
    return vec3(2.0*b*c, a*a - b*b + c*c, -2.0*a*b);
}

subroutine(bitangent_map)
vec3 DiffAndRotMap_B(vec2 final_UV)
{
    vec2 V = 2.0 * texture(rotationMap, final_UV).xy - 1.0;
    vec3 q = 2.0 * texture(quaternionMap, final_UV).xyz - 1.0;
    float a = q.x;
    float b = q.y;
    float c = q.z;
    // float d = 0.0;
    return -V.y * vec3(a*a + b*b - c*c, 2.0*b*c, 2.0*a*c) + V.x * vec3(2.0*b*c, a*a - b*b + c*c, -2.0*a*b);
}

void main(void)
{
    // we determine UVs based on wrapping (repeat) and displacement
    vec3 V = normalize(tViewDirection);
    vec2 disp_UV = Displacement(mod(interp_UV*repeat, 1.0), V);
    vec2 final_UV = mod(disp_UV, 1.0);

    // determine N for Fresnel reflectance calculation (in tangent space)
    vec3 N = normalize(Normal_Map(final_UV));

    // look up metalness and ambient occlusion
    float metallic = texture(metallicMap, final_UV).x;
    float ao = texture(aoMap, final_UV).x;

    vec3 F = vec3(pow(1.0 - dot(V, N), 5.0));
    F *= (1.0 - F0);
    F += F0;

    // ks + kd = 1.0, and we have ks = F
    vec3 kd = 1.0 - F;
//    kd *= 1.0 - metallic;

    // notice ks = F is already included in Specular() calculations
    vec3 color = (kd * Diffuse() + Specular()) * ao;

/*
    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));
*/

    colorFrag = vec4(color, 1.0);
}


// Low discrepancy sequence generation

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}  