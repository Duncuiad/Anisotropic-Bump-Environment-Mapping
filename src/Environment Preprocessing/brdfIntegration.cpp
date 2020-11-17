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

unsigned int floatToIndex(float x, unsigned int size);
void vec2ToGA(glm::vec2 vector, unsigned char here[2]);
glm::vec3 RGBToVec3(unsigned char R, unsigned char G, unsigned char B);

float RadicalInverse_VdC(unsigned int bits);
glm::vec2 Hammersley(unsigned int i, unsigned int N);

int main() 
{
    unsigned int size = 512;
    float nU = 0.0;
    float nV = 0.0;
    const unsigned int SAMPLE_COUNT = 512u;

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
    int sourceWidth, sourceHeight, sourceChannels;
    unsigned char gaBuffer[2];

    unsigned char* image = new unsigned char[3*size*size];
    glm::vec3* halfVectors;

    // check if the file already exists, in which case the program doesn't overwrite it
    while(stbi_info(fullPath.c_str(), NULL, NULL, NULL) != 0) // this file already exists
    {
        current++;
        fullPath = saveName + shininess + " " + std::to_string(current) + "." + format;
    }

    // read the source texture and save the vectors in the halfVector array
    unsigned char *source = stbi_load(sourcePath.c_str(), &sourceWidth, &sourceHeight, &sourceChannels, STBI_rgb);
    if (source == NULL) 
    {
        std::cout << "Error in loading the image: there is no texture called"
                  << std::endl << sourcePath << std::endl;
        exit(1);
    }

    halfVectors = new glm::vec3[sourceWidth*sourceHeight];
    for (int l = 0; l < sourceWidth*sourceHeight; l++)
    {
        halfVectors[l] = RGBToVec3(source[3*l], source[3*l+1], source[3*l+2]);
    }
    stbi_image_free(source);

    // generate the BRDF lookup texture
    for (int j = 0; j < size; j++)
    {

        std::cout << "\rWorking on row " << j + 1 << " of " << size << std::flush;

        for (int i = 0; i < size; i++)
        {

            float TdotV = ((float) i) / ((float) size); // from the left of the image
            float BdotV = ((float) size - j - 1) / ((float) size); // from the bottom of the image
            float TBnormSquared = BdotV*BdotV + TdotV*TdotV;

            if (TBnormSquared >= 1.0) // this doesn't represent a unit vector
            {
                /*
                            unsigned int bufferPosition = 3*(size*j + i);

                            // set this texel to black and continue to the next one
                            image[bufferPosition] = 0;
                            image[bufferPosition + 1] = 0;
                            image[bufferPosition + 2] = 0;

                            continue;
                */
            }
        
            float NdotV = TBnormSquared >= 1.0 ? 0.0 : glm::sqrt(1.0 - TBnormSquared);

            // reconstruct V in tangent space coordinates
            glm::vec3 V;
            V.x = TdotV;
            V.y = BdotV;
            V.z = NdotV;
            V = glm::normalize(V);

            float sizeCoeff = 0.0;
            float biasCoeff = 0.0;

            //float normCoeff = sqrt((nU + 1.0)*(nV + 1.0) / (2.0 * PI));

            for(unsigned int sIndex = 0u; sIndex < SAMPLE_COUNT; sIndex++)
            {
                glm::vec2 Xi = Hammersley(sIndex, SAMPLE_COUNT);
                unsigned int xIndex = floatToIndex(Xi[0], sourceWidth); // from left to right
                unsigned int yIndex = floatToIndex(Xi[1], sourceHeight); // from bottom to top
                unsigned int linearIndex = xIndex + sourceWidth * yIndex; // stride length is sourceWidth
                glm::vec3 H = halfVectors[linearIndex];

                glm::vec3 L = -glm::normalize(glm::reflect(V, H));

                float NdotL = glm::max(L.z, 0.0f);
                //float NdotH = glm::clamp(H.z, 0.0f, 0.999f); // avoid dividing by zero in the exponent division
                //float TdotH = H.x;
                //float BdotH = H.y;
                float VdotH = glm::max(glm::dot(V, H), 0.001f); // avoid dividing by 0 in the reducedBRDF division
/*
                if (NdotV > 0.5)
                    std::cout << "NdotV: " << NdotV << ", NdotL: " << NdotL << 
                    ", V: " << V.x << " " << V.y << " " << V.z << " " <<
                    ", L: " << L.x << " " << L.y << " " << L.z << " " << std::endl;
*/
                if(NdotL > 0.0)
                {
                    // calculate the BRDF
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

            vec2ToGA(glm::vec2(sizeCoeff, biasCoeff), gaBuffer);

            unsigned int bufferPosition = 3*(size*j + i);

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