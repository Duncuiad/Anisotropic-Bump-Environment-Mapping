/*

14_illumination_models_ML_TX.frag: as 12_illumination_models_ML.frag, but with texturing

N.B. 1)  "13_illumination_models_ML_TX.vert" must be used as vertex shader

N.B. 2) In this example, we consider point lights only. For different kind of lights, the computation must be changed (for example, a directional light is defined by the direction of incident light, so the lightDir is passed as uniform and not calculated in the shader like in this case with a point light).

N.B. 3)  the different illumination models are implemented using Shaders Subroutines

N.B. 4)  only Blinn-Phong and GGX illumination models are considered in this shader

N.B. 5) see note 2 in the vertex shader for considerations on multiple lights management

author: Davide Gadia

Real-Time Graphics Programming - a.a. 2019/2020
Master degree in Computer Science
Universita' degli Studi di Milano

*/

#version 410 core

// number of lights in the scene
#define NR_LIGHTS 3

const float PI = 3.14159265359;

// output shader variable
out vec4 colorFrag;

// array with lights incidence directions (calculated in vertex shader, interpolated by rasterization)
in vec3 tLightDirs[NR_LIGHTS];

// vector from fragment to camera (in view coordinate)
in vec3 tViewDirection;

// interpolated texture coordinates
in vec2 interp_UV;

// texture repetitions
uniform float repeat;

// texture sampler
uniform sampler2D tex;

// normal map sampler
uniform sampler2D normMap;

// quaternion map sampler
uniform sampler2D quaternionMap;

// differential map sampler
uniform sampler2D diffMap;

// ambient and specular components (passed from the application)
uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
// weight of the components
// in this case, we can pass separate values from the main application even if Ka+Kd+Ks>1. In more "realistic" situations, I have to set this sum = 1, or at least Kd+Ks = 1, by passing Kd as uniform, and then setting Ks = 1.0-Kd
uniform float Ka;
uniform float Kd;
uniform float Ks;

// shininess coefficients (passed from the application)
uniform float shininess;

// uniforms for GGX model
uniform float alpha; // rugosity - 0 : smooth, 1: rough
uniform float F0; // fresnel reflectance at normal incidence

// uniforms for Ward model
uniform float alphaX; // rugosity along the tangent vector
uniform float alphaY; // rugosity along the bitangent vector

// uniforms for Ashikhmin-Shirley model
// uniform float F0 fresnel reflectance at normal incidence is under GGX
uniform float nX;
uniform float nY;

////////////////////////////////////////////////////////////////////

// the "type" of the Subroutine
subroutine vec4 surface_c(vec2 coord);

// Subroutine Uniform (it is conceptually similar to a C pointer function)
subroutine uniform surface_c Surface_Color;

////////////////////////////////////////////////////////////////////

// the "type" of the Subroutine
subroutine vec4 diffuse_model();

// Subroutine Uniform (it is conceptually similar to a C pointer function)
subroutine uniform diffuse_model Diffuse;

////////////////////////////////////////////////////////////////////

// the "type" of the Subroutine
subroutine vec4 specular_model();

// Subroutine Uniform (it is conceptually similar to a C pointer function)
subroutine uniform specular_model Specular;

////////////////////////////////////////////////////////////////////

// the "type" of the Subroutine
subroutine vec3 normal_map(vec2 repeated_Uv);

// Subroutine Uniform (it is conceptually similar to a C pointer function)
subroutine uniform normal_map Normal_Map;

////////////////////////////////////////////////////////////////////

// the "type" of the Subroutine
subroutine vec3 tangent_map(vec2 repeated_Uv);

// Subroutine Uniform (it is conceptually similar to a C pointer function)
subroutine uniform tangent_map Tangent_Map;

////////////////////////////////////////////////////////////////////

// the "type" of the Subroutine
subroutine vec3 bitangent_map(vec2 repeated_Uv);

// Subroutine Uniform (it is conceptually similar to a C pointer function)
subroutine uniform bitangent_map Bitangent_Map;

////////////////////////////////////////////////////////////////////
// a subroutine for the surface color of the objects
subroutine(surface_c)
vec4 Flat(vec2 coord)
{
    return vec4(diffuseColor, 1.0);
}

// a subroutine for the surface color of the objects, which returns the texel at coordinates UV
subroutine(surface_c)
vec4 Texture(vec2 coord)
{
    return texture(tex, coord);
}

////////////////////////////////////////////////////////////////////
// a subroutine for the diffuse model used by GGX and Ward
subroutine(diffuse_model)
vec4 PBR() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
    // we repeat the UVs and we sample the texture
    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);
    vec4 surfaceColor = texture(tex, repeated_Uv);
    //vec4 surfaceColor = Surface_Color(repeated_Uv);

    vec4 color = vec4(0.0);

    vec3 N = Normal_Map(repeated_Uv);
    
    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // Lambert coefficient
        float NdotL = dot(N,L);

        color += Kd * NdotL * surfaceColor;
    }

    return color/PI;
}

////////////////////////////////////////////////////////////////////
// a subroutine for the Lambert model for multiple lights and texturing
subroutine(diffuse_model)
vec4 Lambert() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
    // we repeat the UVs and we sample the texture
    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);
    vec4 surfaceColor = texture(tex, repeated_Uv);
    //vec4 surfaceColor = Surface_Color(repeated_Uv);

    vec4 color = vec4(Ka*ambientColor,1.0);

    vec3 N = Normal_Map(repeated_Uv);

    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // cosine angle between direction of light and normal
        float NdotL = dot(N,L);

        color += Kd * NdotL * surfaceColor;
    }

    return color;
}

////////////////////////////////////////////////////////////////////
// calculation for Shirley model
float ShirleyLobe(float NdotK)
{
    float base = 1.0 - (NdotK/2.0);
    return 1.0 - pow(base, 5.0);
}

////////////////////////////////////////////////////////////////////
// a subroutine for the Shirley model for diffuse component
subroutine(diffuse_model)
vec4 Shirley() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
    // we repeat the UVs and we sample the texture
    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);
    vec4 surfaceColor = texture(tex, repeated_Uv);
    //vec4 surfaceColor = Surface_Color(repeated_Uv);

    vec4 color = vec4(Ka*ambientColor,1.0);

    vec3 N = Normal_Map(repeated_Uv);
    vec3 V = normalize(tViewDirection);

    float NdotV = dot(N,V);
    float Vfactor = 28.0 * (1.0 - F0) * ShirleyLobe(NdotV) / 23.0 / PI;

    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // cosine angle between direction of light and normal
        float NdotL = max(dot(N,L), 0.0);

        float diffusive = Vfactor * ShirleyLobe(NdotL);

        color += Kd * NdotL * surfaceColor * diffusive;
    }

    return color;
}

/*

////////////////////////////////////////////////////////////////////
// a subroutine for the Shirley model for diffuse component
subroutine(diffuse_model)
vec4 Temporary() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
        // we repeat the UVs and we sample the texture
    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);
    //vec4 surfaceColor = texture(tex, repeated_Uv);
    vec4 surfaceColor = Surface_Color(repeated_Uv);

    return surfaceColor;
}

*/

//////////////////////////////////////////
// a subroutine for the Blinn-Phong model for multiple lights and texturing
subroutine(specular_model)
vec4 BlinnPhong() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{

    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);

    // the specular component
    vec4 specular = vec4(0.0);

    vec3 N = Normal_Map(repeated_Uv);
    
    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // cosine angle between direction of light and normal
        float NdotL = dot(N,L);

        // if the lambert coefficient is positive, then I can calculate the specular component
        if(NdotL > 0.0)
        {
            // the view vector has been calculated in the vertex shader, already negated to have direction from the mesh to the camera
            vec3 V = normalize( tViewDirection );

            // in the Blinn-Phong model we do not use the reflection vector, but the half vector
            vec3 H = normalize(L + V);

            // we use H to calculate the specular component
            float specAngle = max(H.z, 0.0); // in tangent space, N is (0, 0, 1), so H.z == HdotN
            // shininess application to the specular component
            float blinn_phong = pow(specAngle, shininess);

            specular += vec4(Ks * NdotL * blinn_phong * specularColor, 1.0);
        }
    }

    return specular;
}
//////////////////////////////////////////

//////////////////////////////////////////
// Schlick-GGX method for geometry obstruction (used by GGX model)
float G1(float angle, float alpha)
{
    // in case of Image Based Lighting, the k factor is different:
    // usually it is set as k=(alpha*alpha)/2
    float r = (alpha + 1.0);
    float k = (r*r) / 8.0;

    float num   = angle;
    float denom = angle * (1.0 - k) + k;

    return num / denom;
}

//////////////////////////////////////////
// a subroutine for the GGX model for multiple lights and texturing
subroutine(specular_model)
vec4 GGX() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{

    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);

    // we initialize the final color
    vec3 color = vec3(0.0);

    vec3 N = Normal_Map(repeated_Uv);

    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);
    
        // cosine angle between direction of light and normal
        float NdotL = max(dot(N,L), 0.0); // in tangent space, N is (0, 0, 1);

        // we initialize the specular component
        vec3 specular = vec3(0.0);

        // if the cosine of the angle between direction of light and normal is positive, then I can calculate the specular component
        if(NdotL > 0.0)
        {
            // the view vector has been calculated in the vertex shader, already negated to have direction from the mesh to the camera
            vec3 V = normalize( tViewDirection );

            // half vector
            vec3 H = normalize(L + V);

            // we implement the components seen in the slides for a PBR BRDF
            // we calculate the cosines and parameters to be used in the different components
            float NdotH = max(dot(N, H), 0.0);
            float NdotV = max(dot(N, V), 0.0);
            float VdotH = max(dot(V, H), 0.0);
            float alpha_Squared = alpha * alpha;
            float NdotH_Squared = NdotH * NdotH;

            // Geometric factor G2 
            // Smith’s method (uses Schlick-GGX method for both geometry obstruction and shadowing )
            float G2 = G1(NdotV, alpha)*G1(NdotL, alpha);

            // Rugosity D
            // GGX Distribution
            float D = alpha_Squared;
            float denom = (NdotH_Squared*(alpha_Squared-1.0)+1.0);
            D /= PI*denom*denom;

            // Fresnel reflectance F (approx Schlick)
            vec3 F = vec3(pow(1.0 - VdotH, 5.0));
            F *= (1.0 - F0);
            F += F0;

            // we put everything together for the specular component
            specular = (F * G2 * D) / (4.0 * NdotV * NdotL);

            // the rendering equation is:
            // integral of: BRDF * Li * (cosine angle between N and L)
            // BRDF in our case is GGX (this is just the specular component)
            // Li is considered as equal to 1: light is white, and we have not applied attenuation. With colored lights, and with attenuation, the code must be modified and the Li factor must be multiplied to finalColor
            color += specular*NdotL;
        }
    }
    return vec4(color,1.0);
}

//////////////////////////////////////////
// A subroutine for anisotropic specular component, Ashikhmin-Shirley model
subroutine(specular_model)
vec4 AshikhminShirley()
{

    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);

    // we initialize the final color
    vec3 color = vec3(0.0);

    vec3 N = Normal_Map(repeated_Uv);
    vec3 T = Tangent_Map(repeated_Uv);
    vec3 B = Bitangent_Map(repeated_Uv);

    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);
    
        // cosine angle between direction of light and normal
        float NdotL = max(dot(N,L), 0.0);

        // we initialize the specular component
        vec3 specular = vec3(0.0);

        // if the cosine of the angle between direction of light and normal is positive, then I can calculate the specular component
        if(NdotL > 0.0)
        {
            // the view vector has been calculated in the vertex shader, already negated to have direction from the mesh to the camera
            vec3 V = normalize( tViewDirection );

            // half vector
            vec3 H = normalize(L + V);

            // we calculate the cosines and parameters to be used in the different parts of the BRDF
            // all calculations are in tangent space
            float NdotH = dot(N,H);
            float TdotH = dot(T,H);
            float BdotH = dot(B,H);
            float NdotV = max(dot(N,V), 0.0);
            float HdotV = dot(H,V); // == dot(H,L) and it is always positive (when H is defined)

            // TdotH squared, weighted by alpha X
            float wTdotH_sqd = TdotH * TdotH * nX;

            // BdotH squared, weighted by alpha Y
            float wBdotH_sqd = BdotH * BdotH * nY;

            // Fresnel reflectance F (approx Schlick)
            vec3 F = vec3(pow(1.0 - HdotV, 5.0));
            F *= (1.0 - F0);
            F += F0;

            float denom_cosines = max(NdotV, NdotL) * HdotV;
            float normCoeff = sqrt((nX+1.0)*(nY+1.0)) / 8.0 / PI;

            float exponent = (wTdotH_sqd + wBdotH_sqd) / (1.0 - (NdotH*NdotH));

            // we put everything together for the specular component
            specular = vec3(Ks * normCoeff * pow(NdotH, exponent) * F / denom_cosines);

            // the rendering equation is:
            //integral of: BRDF * Li * (cosine angle between N and L)
            // BRDF in our case is Ward
            // Li is considered as equal to 1: light is white, and we have not applied attenuation. With colored lights, and with attenuation, the code must be modified and the Li factor must be multiplied to finalColor
            color += specular*NdotL;
        }

    }
    return vec4(color,1.0);

}

//////////////////////////////////////////
// A subroutine for anisotropic specular component, Heidrich-Seidel model (real-time, not texture mapping)
subroutine(specular_model)
vec4 HeidrichSeidel() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{

    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);

    // ambient component can be calculated at the beginning
    vec4 color = vec4(0.0);

    vec3 N = Normal_Map(repeated_Uv);
    vec3 T = Tangent_Map(repeated_Uv);

    // the view vector has been calculated in the vertex shader, already negated to have direction from the mesh to the camera
    vec3 V = normalize(tViewDirection);
    float TdotV = dot(T,V);
    
    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // cosine angle between direction of light and normal
        float NdotL = max(dot(N,L), 0.0);

        // if the lambert coefficient is positive, then I can calculate the specular component
        if(NdotL > 0.0)
        {
            // cosine angle between the tangent and the direction of light
            float TdotL = L.x; // in tangent space, T is (1, 0, 0)
            // cosine angle between the direction of light and the curve normal chosen by the model
            float LdotNP = sqrt(1 - TdotL*TdotL); // <L,N'>
            // analogous to the BlinnPhong component, but R is calculated
            // reflecting L against N' instead of N
            float VdotR = max(LdotNP*sqrt(1-TdotV*TdotV) - TdotL*TdotV, 0.0);

            // shininess application to the specular component
            float specular = pow(VdotR, shininess);

            // N.B. ): in this implementation, the sum of the components can be different than 1
            color += (vec4(Ks * NdotL * specular * specularColor, 1.0));
        }
    }

    return color;
}

////////////////////////////////////////////////
// A subroutine for anisotropic specular component, Ward model
subroutine(specular_model)
vec4 Ward() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{

    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);

    // we initialize the final color
    vec3 color = vec3(0.0);

    vec3 N = Normal_Map(repeated_Uv);
    vec3 T = Tangent_Map(repeated_Uv);
    vec3 B = Bitangent_Map(repeated_Uv);

    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);
    
        // cosine angle between direction of light and normal
        float NdotL = max(dot(N,L), 0.0);

        // we initialize the specular component
        vec3 specular = vec3(0.0);

        // if the cosine of the angle between direction of light and normal is positive, then I can calculate the specular component
        if(NdotL > 0.0)
        {
            // the view vector has been calculated in the vertex shader, already negated to have direction from the mesh to the camera
            vec3 V = normalize( tViewDirection );

            // half vector
            vec3 H = normalize(L + V);

            // we calculate the cosines and parameters to be used in the different parts of the BRDF
            // all calculations are in tangent space
            float NdotH = dot(N,H);
            float TdotH = dot(T,H);
            float BdotH = dot(B,H);
            float NdotV = max(dot(N,V), 0.0);

            // TdotH squared, weighted by alpha X
            float wTdotH_sqd = TdotH/alphaX;
            wTdotH_sqd = wTdotH_sqd * wTdotH_sqd;

            // BdotH squared, weighted by alpha Y
            float wBdotH_sqd = BdotH/alphaY;
            wBdotH_sqd = wBdotH_sqd * wBdotH_sqd;

            // Fresnel and geometric attenuation aren't contemplated by the Ward model
            // They are substituted by this normalization factor
            float denom_cosines = max(NdotV*NdotL, 0.0001); // avoid dividing by zero along the profile of a mesh
            float normCoeff = sqrt(denom_cosines) * 4.0 * PI * alphaX * alphaY; //normalization coefficient
            normCoeff = 1.0 / normCoeff; // at the denominator

            float exponent = (-2.0)*(wTdotH_sqd + wBdotH_sqd) / (1.0 + NdotH);

            // we put everything together for the specular component
            specular = vec3(Ks * normCoeff * exp(exponent));

            // the rendering equation is:
            //integral of: BRDF * Li * (cosine angle between N and L)
            // BRDF in our case is Ward
            // Li is considered as equal to 1: light is white, and we have not applied attenuation. With colored lights, and with attenuation, the code must be modified and the Li factor must be multiplied to finalColor
            color += specular*NdotL;
        }

    }
    return vec4(color,1.0);

}

////////////////////////////////////////////////////////////////////
// subroutines for assigning N vector, in the abscence or presence of a normal map
subroutine(normal_map)
vec3 Off_N(vec2 repeated_Uv)
{
    return vec3(0.0, 0.0, 1.0);
}

subroutine(normal_map)
vec3 NormalMapping(vec2 repeated_Uv)
{
    return 2.0 * texture(normMap, repeated_Uv).xyz - 1.0;
}

subroutine(normal_map)
vec3 RotationMap_N(vec2 repeated_Uv)
{
    vec3 q = 2.0 * texture(quaternionMap, repeated_Uv).xyz - 1.0;
    float a = q.x;
    float b = q.y;
    float c = q.z;
    // float d = 0.0;
    return vec3(-2.0*a*c, 2.0*a*b, a*a - b*b - c*c);
}

subroutine(tangent_map)
vec3 Off_T(vec2 repeated_Uv)
{
    return vec3(1.0, 0.0, 0.0);
}

subroutine(tangent_map)
vec3 Diff_T(vec2 repeated_Uv)
{
    vec2 V = 2.0 * texture(diffMap, repeated_Uv).xy - 1.0;
    return vec3(V.x, V.y, 0.0);
}

subroutine(tangent_map)
vec3 RotationMap_T(vec2 repeated_Uv)
{
    vec3 q = 2.0 * texture(quaternionMap, repeated_Uv).xyz - 1.0;
    float a = q.x;
    float b = q.y;
    float c = q.z;
    // float d = 0.0;
    return vec3(a*a + b*b - c*c, 2.0*b*c, 2.0*a*c);
}

subroutine(bitangent_map)
vec3 Off_B(vec2 repeated_Uv)
{
    return vec3(0.0, 1.0, 0.0);
}

subroutine(bitangent_map)
vec3 Diff_B(vec2 repeated_Uv)
{
    vec2 V = 2.0 * texture(diffMap, repeated_Uv).xy - 1.0;
    return vec3(-V.y, V.x, 0.0);
}

subroutine(bitangent_map)
vec3 RotationMap_B(vec2 repeated_Uv)
{
    vec3 q = 2.0 * texture(quaternionMap, repeated_Uv).xyz - 1.0;
    float a = q.x;
    float b = q.y;
    float c = q.z;
    // float d = 0.0;
    return vec3(2.0*b*c, a*a - b*b + c*c, -2.0*a*b);
}

//////////////////////////////////////////

// main
void main(void)
{
    colorFrag = Diffuse() + Specular(); 
}
