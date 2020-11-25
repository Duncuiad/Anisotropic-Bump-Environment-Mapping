#include <iostream>
#include <string>

// we load the GLM classes used in the application
#include <glm/glm.hpp>

// we include the library for images loading

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image/stb_image_write.h>

const float PI = 3.14159265359;

// conversion functions to manage reading/writing of vectors on textures
unsigned int floatToIndex(float x, unsigned int size);
void vec2ToGA(glm::vec2 vector, unsigned char here[2]);
glm::vec3 RGBToVec3(unsigned char R, unsigned char G, unsigned char B);

// low discrepancy sequence generation, needed for importance sampling of the half-vectors
float RadicalInverse_VdC(unsigned int bits);
glm::vec2 Hammersley(unsigned int i, unsigned int N);

// -------------- GLOBAL VARIABLES -------------- //

// the width and height of the output texture
unsigned int size = 512;

// directional shininess parameters of Ashikhmin-Shirley anisotropic illumination model
float nU = 1.0;
float nV = 1.0;

// the sample count for importance sampling and Monte-Carlo integration
const unsigned int SAMPLE_COUNT = 1024u;

int main() 
{

    // input management for the parameters nU and nV
    while (std::cout << "Insert value for nU: " && !(std::cin >> nU))
    {
        std::cin.clear(); //clear bad input flag
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); //discard input
        std::cout << "Invalid input; please re-enter.\n";
    }
    while (std::cout << "Insert value for nV: " && !(std::cin >> nV))
    {
        std::cin.clear(); //clear bad input flag
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); //discard input
        std::cout << "Invalid input; please re-enter.\n";
    }

    // automated management of the pathnames of source and output textures, based on nU and nV values
    std::string saveName = "../../textures/brdfIntegration";
    std::string sourceName = "../../textures/halfVectorSampling";
    unsigned int current = 0;
    std::string format = "png";
    std::string sourceFormat = "png";

    std::string strNU = std::to_string(nU);
    strNU.erase(strNU.find_last_not_of('0') + 1, std::string::npos);
    strNU.erase(strNU.find_last_not_of('.') + 1, std::string::npos);
    std::string strNV = std::to_string(nV);
    strNV.erase(strNV.find_last_not_of('0') + 1, std::string::npos);
    strNV.erase(strNV.find_last_not_of('.') + 1, std::string::npos);
    std::string shininess = " [" + strNU + "," + strNV + "]";

    std::string fullPath = saveName + shininess + "." + format;
    std::string sourcePath = sourceName + shininess + "." + sourceFormat;

    // variables for managing reading/writing of textures
    int sourceWidth, sourceHeight, sourceChannels; // values are stored here by the stbi_load call
    unsigned char gaBuffer[2]; // given to Vec2ToGA, which stores the output values for each texel in the buffer

    unsigned char* image = new unsigned char[3*size*size]; // input texture buffer
    glm::vec3* halfVectors; // array of H vectors as read from the source texture

    // check if the file already exists, in which case the program doesn't overwrite it
    while(stbi_info(fullPath.c_str(), NULL, NULL, NULL) != 0) // this file already exists
    {
        current++;

        // incrementally change the name of the file to write onto
        fullPath = saveName + shininess + " " + std::to_string(current) + "." + format;
    }

    // read the source texture
    unsigned char *source = stbi_load(sourcePath.c_str(), &sourceWidth, &sourceHeight, &sourceChannels, STBI_rgb);
    if (source == NULL) 
    {
        std::cout << "Error in loading the image: there is no texture called"
                  << std::endl << sourcePath << std::endl;
        exit(1);
    }

    // save the vectors in the halfVector array
    halfVectors = new glm::vec3[sourceWidth*sourceHeight];
    for (int l = 0; l < sourceWidth*sourceHeight; l++)
    {
        halfVectors[l] = RGBToVec3(source[3*l], source[3*l+1], source[3*l+2]);
    }
    stbi_image_free(source);

    // generate the BRDF lookup texture
    for (int j = 0; j < size; j++) // for each row
    {

        // progress counter
        std::cout << "\rWorking on row " << j + 1 << " of " << size << std::flush;

        // this is the v coordinate of the texture
        float sqrtNdotV = ((float) size - j - 1) / ((float) size); // from the bottom of the image

        float NdotV = glm::clamp(glm::pow(sqrtNdotV, 2.0), 0.0, 1.0); // NdotV = cosTheta

        float sinTheta = sqrt(1.0 - NdotV*NdotV);

        // all of the above values don't change inside each row

        for (int i = 0; i < size; i++) // for each column
        {

            // this is the u coordinate of the texture
            float normalizedPhi = ((float) i) / ((float) size); // from the left of the image

            float phi = normalizedPhi*PI/2.0;

            // phi = normalizedPhi * PI/2 is the angle between 
            float TdotV = glm::cos(phi) * sinTheta;
            float BdotV = glm::sin(phi) * sinTheta;

            // to sum up the calculations, in tangent space we have
            // V = (TdotV, BdotV, NdotV) = (sinTheta * cosPhi, sinTheta * sinPhi, cosTheta)
            // where phi is the angle between T and the projection of V onto the tangent plane
            // and theta is the angle between N and V

            // reconstruct V in tangent space coordinates
            glm::vec3 V;
            V.x = TdotV;
            V.y = BdotV;
            V.z = NdotV;
            V = glm::normalize(V);

            // output values, initialized at 0.0
            float sizeCoeff = 0.0;
            float biasCoeff = 0.0;

            // integration loop
            for(unsigned int sIndex = 0u; sIndex < SAMPLE_COUNT; sIndex++)
            {
                // low discrepancy sequence on the unit square
                glm::vec2 Xi = Hammersley(sIndex, SAMPLE_COUNT);
                unsigned int xIndex = floatToIndex(Xi[0], sourceWidth);
                unsigned int yIndex = floatToIndex(Xi[1], sourceHeight);
                unsigned int linearIndex = xIndex + sourceWidth * yIndex; // stride length is sourceWidth

                // the sequence is mapped from the square to the upper hemisphere via texture lookup
                glm::vec3 H = halfVectors[linearIndex];

                // obtain L as reflection of V against H
                // somehow glm::reflect returns the opposite of the reflection vector:
                // it states it calculates glm::reflect(I,N) = I - 2.0 * dot(I,N) * N;
                // this is weird, but I fix this negating the result;
                glm::vec3 L = -glm::normalize(glm::reflect(V, H));

                // cosines needed for the brdf calculation
                float NdotL = glm::max(L.z, 0.0f);
                float VdotH = glm::clamp(glm::dot(V, H), 0.0f, 1.0f); // avoid raising a negative base in the Fc calculation below

                if(NdotL > 0.0)
                {

                    /*
                    Ashikhmin-Shirley BRDF:
                    p.H(H) = c * NdotH ^ [(nU * TdotH^2 + nV * BdotH^2)/(1 - NdotH^2)], where
                    c = sqrt((nU + 1) * (nV + 1)) / (2*PI)
                    F(VdotH) = F0 + (1 - F0) * (1 - VdotH) ^ 5 = F0 * (1 - (1-VdotH)^5) + (1-VdotH)^5
                    f(H) = 1 / ( 4 * VdotH * max(NdotV,NdotL) )

                    p(L) = p.H(H) / (4 * VdotH), where H is the half-vector for this choice of V and L

                    BRDF(V,L) = p.H(H) * f(H) * F(VdotH)

                    Since we sample L based on probability density p, what we sum in this Monte-Carlo integration is
                    BRDF(V,L) / p(L) = p.H(H) * f(H) * F(VdotH) / p(L);

                    that is, we sum F(VdotH)/max(NdotV, NdotL) (against NdotL)
                    in fact, we split F(VdotH) in the two summands F0 * (1 - (1-VdotH)^5) and (1-VdotH)^5
                    */

                    // calculate the BRDF (having divided by the probability density)
                    float numerator = NdotL;
                    float denominator = glm::max(NdotV, NdotL);
                    float reducedBRDF = numerator/denominator;

                    float Fc = pow(1.0 - VdotH, 5.0);

                    sizeCoeff += (1.0 - Fc) * reducedBRDF;
                    biasCoeff += Fc * reducedBRDF;
                }
            }
            sizeCoeff /= float(SAMPLE_COUNT);
            biasCoeff /= float(SAMPLE_COUNT);

            // save values onto texture
            vec2ToGA(glm::vec2(sizeCoeff, biasCoeff), gaBuffer);

            unsigned int bufferPosition = 3*(size*j + i);

            // I save the image as RGB with null blue component to avoid only having grey and alpha channels, which impedes debugging by sight
            image[bufferPosition] = gaBuffer[0];
            image[bufferPosition + 1] = gaBuffer[1];
            image[bufferPosition + 2] = 0; // blue channel
        }            
    }

    std::cout << std::endl;

    // save the texture
    stbi_write_png(fullPath.c_str(), size, size, STBI_rgb, image, size * STBI_rgb);

    // free space
    delete[] image;
    delete[] halfVectors;

    return 0;
}

unsigned int floatToIndex(float x, unsigned int size)
{
    return (unsigned int) (x * (float) size);
}

void vec2ToGA(glm::vec2 vector, unsigned char here[2])
{
    here[0] = (unsigned char) (vector.x * 255.0);
    here[1] = (unsigned char) (vector.y * 255.0);
}

glm::vec3 RGBToVec3(unsigned char R, unsigned char G, unsigned char B)
{
    float x = ((float) R) / 127.5 - 1.0;
    float y = ((float) G) / 127.5 - 1.0;
    float z = ((float) B) / 127.5 - 1.0;
    return glm::vec3(x, y, z);
}

// Low discrepancy sequence generation:

float RadicalInverse_VdC(unsigned int bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

glm::vec2 Hammersley(unsigned int i, unsigned int N)
{
    return glm::vec2(float(i)/float(N), RadicalInverse_VdC(i));
}