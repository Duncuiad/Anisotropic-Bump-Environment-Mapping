#include <iostream>

// we load the GLM classes used in the application
#include <glm/glm.hpp>

// we include the library for images loading
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image/stb_image_write.h>

glm::vec3 RGBToVec3(unsigned char R, unsigned char G, unsigned char B);

void vec3ToRGB(glm::vec3 vector, unsigned char *here);

glm::vec3 RotationQuaternion(glm::vec3 perturbedNormal);

int main(int argc, char *argv) 
{
    char* path = argv;
    int width, height, channels;
    unsigned char rgbBuffer[3];
    unsigned char *img = stbi_load("../../textures/hammered_metal/Metal_Hammered_002_4K_normal.jpg", &width, &height, &channels, STBI_rgb);
    if(img == NULL) 
    {
        std::cout << "Error in loading the image" << std::endl;
        exit(1);
    }

    for (int i = 0; i < width*height; i++)
    {
        glm::vec3 normalMap = RGBToVec3(img[3*i], img[3*i+1], img[3*i+2]);
        glm::vec3 quaternion = RotationQuaternion(normalMap);
        vec3ToRGB(quaternion, rgbBuffer);

        img[3*i] = rgbBuffer[0];
        img[3*i+1] = rgbBuffer[1];
        img[3*i+2] = rgbBuffer[2];
    }

    stbi_write_png("../../textures/quaternionRotation.png", width, height, STBI_rgb, img, width * STBI_rgb);

    stbi_image_free(img);
    return 0;
}

glm::vec3 RGBToVec3(unsigned char R, unsigned char G, unsigned char B)
{
    float x = ((float) R) / 127.5 - 1.0;
    float y = ((float) G) / 127.5 - 1.0;
    float z = ((float) B) / 127.5 - 1.0;
    return glm::vec3(x, y, z);
}

void vec3ToRGB(glm::vec3 vector, unsigned char *here)
{
    here[0] = (unsigned char) (floorf(vector.x * 127.5 + 127.5));
    here[1] = (unsigned char) (floorf(vector.y * 127.5 + 127.5));
    here[2] = (unsigned char) (floorf(vector.z * 127.5 + 127.5));
}

glm::vec3 RotationQuaternion(glm::vec3 perturbedNormal)
{
    float a = sqrt((perturbedNormal.z + 1.0)/2.0);
    float b = perturbedNormal.y / (2.0*a);
    float c = -perturbedNormal.x / (2.0*a);
    // float d = 0;
    return glm::vec3(a, b, c);
}