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

/*
// normal map sampler
uniform sampler2D normap;
*/

// ambient and specular components (passed from the application)
uniform vec3 ambientColor;
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

////////////////////////////////////////////////////////////////////

// the "type" of the Subroutine
subroutine vec4 diffuse_model();

// Subroutine Uniform (it is conceptually similar to a C pointer function)
subroutine uniform diffuse_model Diffuse;

////////////////////////////////////////////////////////////////////

subroutine vec4 specular_model();

// Subroutine Uniform (it is conceptually similar to a C pointer function)
subroutine uniform specular_model Specular;

////////////////////////////////////////////////////////////////////
// a subroutine for the diffuse model used by GGX and Ward
subroutine(diffuse_model)
vec4 PBR() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
    // we repeat the UVs and we sample the texture
    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);
    vec4 surfaceColor = texture(tex, repeated_Uv);

    vec4 color = vec4(0.0);
    
    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // Lambert coefficient
        float NdotL = L.z; // in tangent space, N is (0, 0, 1);

        color += Kd * NdotL * surfaceColor;
    }

    return color/PI;
}

// a subroutine for the Lambert model for multiple lights and texturing
subroutine(diffuse_model)
vec4 Lambert() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
    // we repeat the UVs and we sample the texture
    vec2 repeated_Uv = mod(interp_UV*repeat, 1.0);
    vec4 surfaceColor = texture(tex, repeated_Uv);

    vec4 color = vec4(Ka*ambientColor,1.0);

    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // cosine angle between direction of light and normal
        float NdotL = L.z; // in tangent space, N is (0, 0, 1);

        color += Kd * NdotL * surfaceColor;
    }

    return color;
}

//////////////////////////////////////////
// a subroutine for the Blinn-Phong model for multiple lights and texturing
subroutine(specular_model)
vec4 BlinnPhong() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
    // the specular component
    vec4 specular = vec4(0.0);
    
    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // cosine angle between direction of light and normal
        float NdotL = L.z; // in tangent space, N is (0, 0, 1);

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

            specular += vec4(Ks * blinn_phong * specularColor, 1.0);
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

    // we initialize the final color
    vec3 color = vec3(0.0);

    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);
    
        // cosine angle between direction of light and normal
        float NdotL = max(L.z, 0.0); // in tangent space, N is (0, 0, 1);

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
            float NdotH = max(H.z, 0.0);
            float NdotV = max(V.z, 0.0);
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
// A subroutine for anisotropic specular component, Heidrich-Seidel model (real-time, not texture mapping)
subroutine(specular_model)
vec4 HeidrichSeidel() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{

    // ambient component can be calculated at the beginning
    vec4 color = vec4(0.0);

    // the view vector has been calculated in the vertex shader, already negated to have direction from the mesh to the camera
    vec3 V = normalize(tViewDirection);
    float TdotV = V.x; // in tangent space, T is (1, 0, 0)
    
    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);

        // cosine angle between direction of light and normal
        float NdotL = max(L.z, 0.0); // in tangent space, N is (0, 0, 1);

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
            color += NdotL*(vec4(Ks * specular * specularColor, 1.0));
        }
    }

    return color;
}

////////////////////////////////////////////////
// A subroutine for anisotropic specular component, Ward model
subroutine(specular_model)
vec4 Ward() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{

    // we initialize the final color
    vec3 color = vec3(0.0);

    //for all the lights in the scene
    for(int i = 0; i < NR_LIGHTS; i++)
    {
        // normalization of the per-fragment light incidence direction
        vec3 L = normalize(tLightDirs[i]);
    
        // cosine angle between direction of light and normal
        float NdotL = max(L.z, 0.0);

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
            float NdotH = H.z;
            float TdotH = H.x;
            float BdotH = H.y;
            float NdotV = max(V.z, 0.0);

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

//////////////////////////////////////////

// main
void main(void)
{
    colorFrag = Diffuse() + Specular(); 
}
