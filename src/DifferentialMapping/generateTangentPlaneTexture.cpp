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

glm::vec3 RGBToVec3(unsigned char R, unsigned char G, unsigned char B);
glm::vec4 RGBAToVec4(unsigned char R, unsigned char G, unsigned char B, unsigned char A);

void vec3ToRGB(glm::vec3 vector, unsigned char here[3]);
void vec4ToRGBA(glm::vec4 vector, unsigned char here[4]);

int main() 
{
    unsigned int size = 1024;

    std::string saveName = "../../textures/tangentPlaneMapping";
    unsigned int current = 0;
    std::string format = "png";
    std::string fullPath = saveName + "." + format;
    unsigned char rgbaBuffer[4];

    unsigned char* image = new unsigned char[4*size*size];

    // check if the file already exists, in which case the program doesn't overwrite it
    while(stbi_info(fullPath.c_str(), NULL, NULL, NULL) != 0) // this file already exists
    {
        current++;
        fullPath = saveName + "_" + std::to_string(current) + "." + format;
    }

    // generate the texture
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            float u = ((float) i) / ((float) size);
            float v = ((float) size - j - 1) / ((float) size);

            // THESE ARE THE VALUES FOR THE TEXTURE
            float x = glm::cos(PI*v); // x(u,v)
            float y = glm::sin(PI*v); // y(u,v)
            float m = 1.0 - 0.95*sqrt(glm::sin(PI*v)); // principal roughness
            float n = 1.0; // orthogonal roughness
            // ------------------------------------

            glm::vec4 transformed(x, y, m, n);

            vec4ToRGBA(transformed, rgbaBuffer);

            unsigned int bufferPosition = 4*(size*j + i);

            image[bufferPosition] = rgbaBuffer[0];
            image[bufferPosition + 1] = rgbaBuffer[1];
            image[bufferPosition + 2] = rgbaBuffer[2];
            image[bufferPosition + 3] = rgbaBuffer[3];
        }            
    }

    // save the texture
    stbi_write_png(fullPath.c_str(), size, size, STBI_rgb_alpha, image, size * STBI_rgb_alpha);

    // free space
    delete[] image;

    return 0;
}

glm::vec3 RGBToVec3(unsigned char R, unsigned char G, unsigned char B)
{
    float x = ((float) R) / 127.5 - 1.0;
    float y = ((float) G) / 127.5 - 1.0;
    float z = ((float) B) / 127.5 - 1.0;
    return glm::vec3(x, y, z);
}

glm::vec4 RGBAToVec4(unsigned char R, unsigned char G, unsigned char B, unsigned char A)
{
    float x = ((float) R) / 127.5 - 1.0;
    float y = ((float) G) / 127.5 - 1.0;
    float z = ((float) B) / 127.5 - 1.0;
    float w = ((float) A) / 127.5 - 1.0;
    return glm::vec4(x, y, z, w);
}

void vec3ToRGB(glm::vec3 vector, unsigned char here[3])
{
    here[0] = (unsigned char) (floorf(vector.x * 127.5 + 127.5));
    here[1] = (unsigned char) (floorf(vector.y * 127.5 + 127.5));
    here[2] = (unsigned char) (floorf(vector.z * 127.5 + 127.5));
}

void vec4ToRGBA(glm::vec4 vector, unsigned char here[4])
{
    here[0] = (unsigned char) (floorf(vector.x * 127.5 + 127.5));
    here[1] = (unsigned char) (floorf(vector.y * 127.5 + 127.5));
    here[2] = (unsigned char) (floorf(vector.z * 127.5 + 127.5));
    here[3] = (unsigned char) (floorf(vector.w * 127.5 + 127.5));
}