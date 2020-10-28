/*
Es04: as Es03d, but textures are used for the diffusive component of materials
- Moreover, it is possible to move inside the scene using WASD keys and mouse.

N.B. 1) 
In this example we use Shaders Subroutines to do shader swapping:
http://www.geeks3d.com/20140701/opengl-4-shader-subroutines-introduction-3d-programming-tutorial/
https://www.lighthouse3d.com/tutorials/glsl-tutorial/subroutines/
https://www.khronos.org/opengl/wiki/Shader_Subroutine

In other cases, an alternative could be to consider Separate Shader Objects:
https://www.informit.com/articles/article.aspx?p=2731929&seqNum=7
https://www.khronos.org/opengl/wiki/Shader_Compilation#Separate_programs
https://riptutorial.com/opengl/example/26979/load-separable-shader-in-cplusplus

N.B. 2) 
There are other methods (more efficient) to pass multiple data to the shaders, using for example Uniform Buffer Objects.
With last versions of OpenGL, using structures like the one cited above, it is possible to pass a "dynamic" number of lights
https://www.geeks3d.com/20140704/gpu-buffers-introduction-to-opengl-3-1-uniform-buffers-objects/
https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL (scroll down a bit)
https://hub.packtpub.com/opengl-40-using-uniform-blocks-and-uniform-buffer-objects/

N.B. 3) we have considered point lights only, the code must be modified for different light sources

N.B. 4) to test different parameters of the shaders, it is convenient to use some GUI library, like e.g. Dear ImGui (https://github.com/ocornut/imgui)

N.B. 5) The Camera class has been added in include/utils

author: Davide Gadia

Real-Time Graphics Programming - a.a. 2019/2020
Master degree in Computer Science
Universita' degli Studi di Milano
*/

/*
OpenGL coordinate system (right-handed)
positive X axis points right
positive Y axis points up
positive Z axis points "outside" the screen


                              Y
                              |
                              |
                              |________X
                             /
                            /
                           /
                          Z
*/

#ifdef _WIN32
    #define __USE_MINGW_ANSI_STDIO 0
#endif
// Std. Includes
#include <string>

// Loader for OpenGL extensions
// http://glad.dav1d.de/
// THIS IS OPTIONAL AND NOT REQUIRED, ONLY USE THIS IF YOU DON'T WANT GLAD TO INCLUDE windows.h
// GLAD will include windows.h for APIENTRY if it was not previously defined.
// Make sure you have the correct definition for APIENTRY for platforms which define _WIN32 but don't use __stdcall
#ifdef _WIN32
    #define APIENTRY __stdcall
#endif

#include <glad/glad.h>

// GLFW library to create window and to manage I/O
#include <glfw/glfw3.h>

// another check related to OpenGL loader
// confirm that GLAD didn't include windows.h
#ifdef _WINDOWS_
    #error windows.h was included!
#endif

// classes developed during lab lectures to manage shaders, to load models, and for FPS camera
// in this example, the Model and Mesh classes support texturing
#include <utils/shader_v1.h>
#include <utils/model_v2.h>
#include <utils/camera.h>

// we load the GLM classes used in the application
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

// we include the library for images loading
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

// we include Dear ImGui for parameter tweaking and performance evaluation
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

// number of lights in the scene
#define NR_LIGHTS 3

// custom max number of subroutine uniforms
#define MY_MAX_SUB_UNIF 32


// dimensions of application's window
GLuint screenWidth = 800, screenHeight = 600;

// dimensions of Dear ImGui window
GLuint guiWidth = 800, guiHeight = 600;

// callback functions for keyboard and mouse events
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
// if one of the WASD keys is pressed, we call the corresponding method of the Camera class
void apply_camera_movements();

// 
GLint countActiveSU;
// GLint current_sub_uniform = 0;
// a vector counting the number of compatible subroutines for each subroutine uniform
vector<int> num_compatible_subroutines;
// the vector storing the current subroutine for each uniform
vector<GLuint> current_subroutines;
// a vector of the list of indices of compatible subroutines for each subroutine uniform
vector<int*> compatible_subroutines;
// a vector for all the shader subroutine uniforms names
vector<std::string> sub_uniforms_names;
// a vector for all the shader subroutines names used and swapped in the application
vector<std::string> subroutines_names;

// the name of the subroutines are searched in the shaders, and placed in the shaders vector (to allow shaders swapping)
void SetupShader(int shader_program);

// print on console the name of current shader subroutine
void PrintCurrentShader(int subroutine);

// load image from disk and create an OpenGL texture
GLint LoadTexture(const char* path);

// CUSTOM OVERLOAD
// ImGui::RadioButton is commented FIXME in source imgui_widget.cpp
// Issue is it only accepts int*, int as parameters, while we need GLuint to read and write with [GLuint current_subroutine]
// The developer asks to overload function with template type template<T>, having T* and T as parameters, "but I'm not sure how we would expose it.."
// @Overload
namespace ImGui {
    bool RadioButton(const char* label, GLuint* v, GLuint v_button);
}


// we initialize an array of booleans for each keybord key
bool keys[1024];

// we need to store the previous mouse position to calculate the offset with the current frame
GLfloat lastX, lastY;

// when rendering the first frame, we do not have a "previous state" for the mouse, so we need to manage this situation
bool firstMouse = true;
// boolean to activate/deactivate mouse cursor
GLboolean cursorActive = GL_FALSE;

// parameters for time calculation (for animations)
GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

// rotation angle on Y axis
GLfloat orientationY = 0.0f;
// rotation speed on Y axis
GLfloat spin_speed = 30.0f;
// boolean to start/stop animated rotation on Y angle
GLboolean spinning = GL_TRUE;

// boolean to activate/deactivate wireframe rendering
GLboolean wireframe = GL_FALSE;

// we create a camera. We pass the initial position as a paramenter to the constructor. The last boolean tells that we want a camera "anchored" to the ground
Camera camera(glm::vec3(0.0f, 0.0f, 7.0f), GL_TRUE);

// Uniforms to be passed to shaders
// pointlights positions
glm::vec3 lightPositions[] = {
    glm::vec3(5.0f, 10.0f, 10.0f),
//    glm::vec3(-5.0f, 10.0f, 10.0f),
//    glm::vec3(5.0f, 10.0f, -10.0f),
};

// specular and ambient components
GLfloat specularColor[] = {1.0,1.0,1.0};
GLfloat ambientColor[] = {0.1,0.1,0.1};
// weights for the diffusive, specular and ambient components
GLfloat Ka = 0.2f, Kd = 0.8f, Ks = 0.5f;
//GLfloat lightingComponents[] = {0.2f, 0.8f, 0.5f}; // {Ka, Kd, Ks};
// shininess coefficient for Blinn-Phong shader
GLfloat shininess = 25.0f;

// roughness index for Cook-Torrance shader
GLfloat alpha = 0.2f;
// Fresnel reflectance at 0 degree (Schlik's approximation)
GLfloat F0 = 0.9f;

// directional roughnesses for Ward shader [to be pointed to by ImGui sliders]
GLfloat alphaX = 0.1f, alphaY = 1.0f;

// directional shininess(es) for Ashikhmin-Shirley model
GLfloat nX = 1.0f, nY = 10.0f;

// vector for the textures IDs
vector<GLint> textureID;

// UV repetitions
GLfloat repeat = 1.0f;

/////////////////// MAIN function ///////////////////////
int main()
{
  // Initialization of OpenGL context using GLFW
  glfwInit();
  // We set OpenGL specifications required for this application
  // In this case: 4.1 Core
  // If not supported by your graphics HW, the context will not be created and the application will close
  // N.B.) creating GLAD code to load extensions, try to take into account the specifications and any extensions you want to use,
  // in relation also to the values indicated in these GLFW commands
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  const char* glsl_version = "#version 410";
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  // we set if the window is resizable
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  // we create the ImGui window
    GLFWwindow* guiWindow = glfwCreateWindow(guiWidth, guiHeight, "Dear ImGui", nullptr, nullptr);
    if (!guiWindow)
    {
        std::cout << "Failed to create GLFW gui window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwSetWindowPos(guiWindow, screenWidth, 30);
    glfwMakeContextCurrent(guiWindow);

  // we create the application's window
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Anisotropic with (only) normal mapping", nullptr, nullptr);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwSetWindowPos(window, 0, 30);
    glfwMakeContextCurrent(window);

    // we put in relation the window and the callbacks
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    

    // we disable the mouse cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLAD tries to load the context set by GLFW
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        std::cout << "Failed to initialize OpenGL context" << std::endl;
        return -1;
    }


  // setup Dear ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(guiWindow, true);
    ImGui_ImplOpenGL3_Init(glsl_version);


// SCENE SETUP

    // we define the viewport dimensions
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // we enable Z test
    glEnable(GL_DEPTH_TEST);

    //the "clear" color for the frame buffer
    ImVec4 clear_color = ImVec4(0.26f, 0.46f, 0.98f, 1.0f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

    // we create the Shader Program used for objects (which presents different subroutines we can switch)
    Shader illumination_shader = Shader("aniso_normonly_3.vert", "aniso_normonly_3.frag");
    // we parse the Shader Program to search for the number and names of the subroutines. 
    // the names are placed in the shaders vector
    SetupShader(illumination_shader.Program);
    // we print on console the name of the first subroutine used
    // we load the model(s) (code of Model class is in include/utils/model_v2.h)
    Model cubeModel("../../models/cube.obj");
    Model sphereModel("../../models/sphere.obj");
    Model bunnyModel("../../models/bunny_lp.obj");
    Model planeModel("../../models/plane.obj");

    // we load the images and store them in a vector
    textureID.push_back(LoadTexture("../../textures/hammered_metal/Metal_Hammered_002_4K_basecolor.jpg"));
    textureID.push_back(LoadTexture("../../textures/SoilCracked.png"));
    textureID.push_back(LoadTexture("../../textures/hammered_metal/Metal_Hammered_002_4K_normal.jpg"));

    // Projection matrix: FOV angle, aspect ratio, near and far planes
    glm::mat4 projection = glm::perspective(45.0f, (float)screenWidth/(float)screenHeight, 0.1f, 10000.0f);

    // View matrix: the camera moves, so we just set to indentity now
    glm::mat4 view = glm::mat4(1.0f);

    // Model and Normal transformation matrices for the objects in the scene: we set to identity
    glm::mat4 sphereModelMatrix = glm::mat4(1.0f);
    glm::mat3 sphereNormalMatrix = glm::mat3(1.0f);
    glm::mat4 cubeModelMatrix = glm::mat4(1.0f);
    glm::mat3 cubeNormalMatrix = glm::mat3(1.0f);
    glm::mat4 bunnyModelMatrix = glm::mat4(1.0f);
    glm::mat3 bunnyNormalMatrix = glm::mat3(1.0f);
    glm::mat4 planeModelMatrix = glm::mat4(1.0f);
    glm::mat3 planeNormalMatrix = glm::mat3(1.0f);

    // Rendering loop: this code is executed at each frame
    while(!glfwWindowShouldClose(window))
    {

        // SCENE RENDERING

        glfwMakeContextCurrent(window);

        // we determine the time passed from the beginning
        // and we calculate time difference between current frame rendering and the previous one
        GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Check fs an I/O event is happening
        glfwPollEvents();
        // we apply FPS camera movements if mouse cursor is disabled
        if (!cursorActive)
            apply_camera_movements();
        // View matrix (=camera): position, view direction, camera "up" vector
        view = camera.GetViewMatrix();

        // we "clear" the frame and z buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // we set the rendering mode
        if (wireframe)
            // Draw in wireframe
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // if animated rotation is activated, than we increment the rotation angle using delta time and the rotation speed parameter
        if (spinning)
            orientationY+=(deltaTime*spin_speed);

        /////////////////// PLANE ////////////////////////////////////////////////
         // We render a plane under the objects. We apply the Blinn-Phong model only, and we do not apply the rotation applied to the other objects.
        illumination_shader.Use();
        // we search inside the Shader Program the name of the subroutine, and we get the numerical index
        GLuint index_diffuse = glGetSubroutineIndex(illumination_shader.Program, GL_FRAGMENT_SHADER, "Lambert");
        GLuint index_specular = glGetSubroutineIndex(illumination_shader.Program, GL_FRAGMENT_SHADER, "BlinnPhong");
        GLuint plane_indices[2] =  {index_diffuse, index_specular};
        // we activate the subroutine using the index (this is where shaders swapping happens)
        glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 2, &plane_indices[0]);

        // we determine the position in the Shader Program of the uniform variables
        GLint textureLocation = glGetUniformLocation(illumination_shader.Program, "tex");
        GLint normalLocation = glGetUniformLocation(illumination_shader.Program, "normMap");
        GLint hasNormalLocation = glGetUniformLocation(illumination_shader.Program, "hasNormalMap");
        GLint repeatLocation = glGetUniformLocation(illumination_shader.Program, "repeat");
        GLint matAmbientLocation = glGetUniformLocation(illumination_shader.Program, "ambientColor");
        GLint matSpecularLocation = glGetUniformLocation(illumination_shader.Program, "specularColor");
        GLint kaLocation = glGetUniformLocation(illumination_shader.Program, "Ka");
        GLint kdLocation = glGetUniformLocation(illumination_shader.Program, "Kd");
        GLint ksLocation = glGetUniformLocation(illumination_shader.Program, "Ks");
        GLint shineLocation = glGetUniformLocation(illumination_shader.Program, "shininess");
        GLint alphaLocation = glGetUniformLocation(illumination_shader.Program, "alpha");
        GLint f0Location = glGetUniformLocation(illumination_shader.Program, "F0");
        GLint alphaXLocation = glGetUniformLocation(illumination_shader.Program, "alphaX");
        GLint alphaYLocation = glGetUniformLocation(illumination_shader.Program, "alphaY");
        GLint nXLocation = glGetUniformLocation(illumination_shader.Program, "nX");
        GLint nYLocation = glGetUniformLocation(illumination_shader.Program, "nY");
            
        // we assign the value to the uniform variables
        glUniform3fv(matAmbientLocation, 1, ambientColor);
        glUniform3fv(matSpecularLocation, 1, specularColor);
        glUniform1f(shineLocation, shininess);
        glUniform1f(alphaLocation, alpha);
        glUniform1f(f0Location, F0);
        glUniform1f(alphaXLocation, alphaX);
        glUniform1f(alphaYLocation, alphaY);
        glUniform1f(nXLocation, nX);
        glUniform1f(nYLocation, nY);

        // for the plane, we make it mainly Lambertian, by setting at 0 the specular component
        glUniform1f(kaLocation, 0.0f);
        glUniform1f(kdLocation, 0.6f);
        glUniform1f(ksLocation, 0.0f);
        
        // we pass projection and view matrices to the Shader Program
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "projectionMatrix"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "viewMatrix"), 1, GL_FALSE, glm::value_ptr(view));

        // we pass each light position to the shader
        for (GLuint i = 0; i < NR_LIGHTS; i++)
        {
            string number = to_string(i);
            glUniform3fv(glGetUniformLocation(illumination_shader.Program, ("lights[" + number + "]").c_str()), 1, glm::value_ptr(lightPositions[i]));
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureID[1]);

        // we use a different texture used and the number of repetitions for the plane
        glUniform1i(textureLocation, 1);
        glUniform1f(repeatLocation, 80.0f);
        glUniform1i(hasNormalLocation, GL_FALSE);

        // we create the transformation matrix
        // we reset to identity at each frame
        planeModelMatrix = glm::mat4(1.0f);
        planeNormalMatrix = glm::mat3(1.0f);
        planeModelMatrix = glm::translate(planeModelMatrix, glm::vec3(0.0f, -1.0f, 0.0f));
        planeModelMatrix = glm::scale(planeModelMatrix, glm::vec3(10.0f, 1.0f, 10.0f));
        planeNormalMatrix = glm::inverseTranspose(glm::mat3(view*planeModelMatrix));
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(planeModelMatrix));
        glUniformMatrix3fv(glGetUniformLocation(illumination_shader.Program, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(planeNormalMatrix));

        // we render the plane
        planeModel.Draw();


        /////////////////// OBJECTS ////////////////////////////////////////////////
        // we search inside the Shader Program the name of the subroutine currently selected, and we get the numerical index
        GLuint indices[MY_MAX_SUB_UNIF];
        for (int i = 0; i < countActiveSU; i++) {
            indices[i] = glGetSubroutineIndex(illumination_shader.Program, GL_FRAGMENT_SHADER, subroutines_names[current_subroutines[i]].c_str());
        }
        // we activate the desired subroutines using the indices (this is where shaders swapping happens)
        glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, countActiveSU, &indices[0]);

        // Textures and Normal Maps
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID[0]);

        // we change texture for the objects 
        glUniform1i(textureLocation, 0);
        glUniform1f(repeatLocation, repeat);

        // normal map
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textureID[2]);
        glUniform1i(normalLocation, 2);
        glUniform1i(hasNormalLocation, GL_TRUE);

        // we set other parameters for the objects
        glUniform1f(kaLocation, Ka);
        glUniform1f(kdLocation, Kd);
        glUniform1f(ksLocation, Ks);

        // SPHERE
        /*
          we create the transformation matrix

          N.B.) the last defined is the first applied

          We need also the matrix for normals transformation, which is the inverse of the transpose of the 3x3 submatrix (upper left) of the modelview. We do not consider the 4th column because we do not need translations for normals.
          An explanation (where XT means the transpose of X, etc):
            "Two column vectors X and Y are perpendicular if and only if XT.Y=0. If We're going to transform X by a matrix M, we need to transform Y by some matrix N so that (M.X)T.(N.Y)=0. Using the identity (A.B)T=BT.AT, this becomes (XT.MT).(N.Y)=0 => XT.(MT.N).Y=0. If MT.N is the identity matrix then this reduces to XT.Y=0. And MT.N is the identity matrix if and only if N=(MT)-1, i.e. N is the inverse of the transpose of M.

        */
       // we reset to identity at each frame
        sphereModelMatrix = glm::mat4(1.0f);
        sphereNormalMatrix = glm::mat3(1.0f);
        sphereModelMatrix = glm::translate(sphereModelMatrix, glm::vec3(-3.0f, 0.0f, 0.0f));
        sphereModelMatrix = glm::rotate(sphereModelMatrix, glm::radians(orientationY), glm::vec3(0.0f, 1.0f, 0.0f));
        sphereModelMatrix = glm::scale(sphereModelMatrix, glm::vec3(0.8f, 0.8f, 0.8f));
        // if we cast a mat4 to a mat3, we are automatically considering the upper left 3x3 submatrix
        sphereNormalMatrix = glm::inverseTranspose(glm::mat3(view*sphereModelMatrix));
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(sphereModelMatrix));
        glUniformMatrix3fv(glGetUniformLocation(illumination_shader.Program, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(sphereNormalMatrix));

        // we render the sphere
        sphereModel.Draw();

        //CUBE
        // we create the transformation matrix and the normals transformation matrix
        // we reset to identity at each frame
        cubeModelMatrix = glm::mat4(1.0f);
        cubeNormalMatrix = glm::mat3(1.0f);
        cubeModelMatrix = glm::translate(cubeModelMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        cubeModelMatrix = glm::rotate(cubeModelMatrix, glm::radians(orientationY), glm::vec3(0.0f, 1.0f, 0.0f));
        cubeModelMatrix = glm::scale(cubeModelMatrix, glm::vec3(0.8f, 0.8f, 0.8f));
        cubeNormalMatrix = glm::inverseTranspose(glm::mat3(view*cubeModelMatrix));
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(cubeModelMatrix));
        glUniformMatrix3fv(glGetUniformLocation(illumination_shader.Program, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(cubeNormalMatrix));

        // we render the cube
        cubeModel.Draw();

        //BUNNY
        // we create the transformation matrix and the normals transformation matrix
        // we reset to identity at each frame
        bunnyModelMatrix = glm::mat4(1.0f);
        bunnyNormalMatrix = glm::mat3(1.0f);
        bunnyModelMatrix = glm::translate(bunnyModelMatrix, glm::vec3(3.0f, 0.0f, 0.0f));
        bunnyModelMatrix = glm::rotate(bunnyModelMatrix, glm::radians(orientationY), glm::vec3(0.0f, 1.0f, 0.0f));
        bunnyModelMatrix = glm::scale(bunnyModelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
        bunnyNormalMatrix = glm::inverseTranspose(glm::mat3(view*bunnyModelMatrix));
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(bunnyModelMatrix));
        glUniformMatrix3fv(glGetUniformLocation(illumination_shader.Program, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(bunnyNormalMatrix));

        // we render the bunny
        bunnyModel.Draw();

        glfwSwapBuffers(window);

        // GUI RENDERING

        glfwMakeContextCurrent(guiWindow);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            

            ImGui::Begin("Scene GUI");


            ImGui::Text("Shaders");
            ImGui::Indent();
            for (GLuint i = 0; i < countActiveSU; i++) 
            {
                ImGui::BulletText(sub_uniforms_names[i].c_str());

                ImGui::Indent();
                for (GLuint j = 0; j < num_compatible_subroutines[i]; j++) {
                    ImGui::RadioButton(subroutines_names[compatible_subroutines[i][j]].c_str(), &current_subroutines[i], compatible_subroutines[i][j]);
                }
                ImGui::Unindent();
            }
            ImGui::Unindent();
            
            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();

            if (ImGui::TreeNode("Settings"))
            {
                if (ImGui::TreeNode("Lighting Components"))
                {
                    ImGui::SliderFloat("Ambient", &Ka, 0.0, 1.0, "Ka = %.2f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Diffuse", &Kd, 0.0, 1.0, "Kd = %.2f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Specular", &Ks, 0.0, 1.0, "Ks = %.2f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Blinn-Phong and Heidrich-Seidel"))
                {
                    ImGui::SliderFloat("Shininess", &shininess, 1.0, 1000.0, "n = %.2f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Shirley"))
                {
                    ImGui::SliderFloat("Normal incidence Fresnel reflectance", &F0, 0.0001f, 1.0f, "F0 = %.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("GGX"))
                {
                    ImGui::SliderFloat("alpha", &alpha, 0.0001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Normal incidence Fresnel reflectance", &F0, 0.0001f, 1.0f, "F0 = %.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Ashikhmin-Shirley"))
                {
                    ImGui::SliderFloat("Normal incidence Fresnel reflectance", &F0, 0.0001f, 1.0f, "F0 = %.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("X-shininess", &nX, 1.0, 1000.0, "nX = %.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp); 
                    ImGui::SliderFloat("Y-shininess", &nY, 1.0, 1000.0, "nY = %.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp); 
                    if (ImGui::Button("Swap X and Y shininess"))    // Buttons return true when clicked (most widgets return true when edited/activated)
                    {
                        float temp = nX;
                        nX = nY;
                        nY = temp;
                    }
                    ImGui::TreePop();
                }
                
                if (ImGui::TreeNode("Ward"))
                {
                    ImGui::SliderFloat("alpha X", &alphaX, 0.01f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp); 
                    ImGui::SliderFloat("alpha Y", &alphaY, 0.01f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::Button("Swap X and Y alphas"))    // Buttons return true when clicked (most widgets return true when edited/activated)
                    {
                        float temp = alphaX;
                        alphaX = alphaY;
                        alphaY = temp;
                    }
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Metrics"))
            {
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::Text("Application delta_time %.3f ms/frame (%.1f FPS)", deltaTime * 1000, deltaTime == 0 ? 0 : 1/deltaTime);
                ImGui::TreePop();
            }
            

            ImGui::End();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(guiWindow, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swapping back and front buffers
        glfwSwapBuffers(guiWindow);
    }

   // when I exit from the graphics loop, it is because the application is closing
    // we delete the Shader Program
    illumination_shader.Delete();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // deallocate memory used by dynamic arrays
    for (int i = 0; i < countActiveSU; i++)
    {
        delete[] compatible_subroutines[i];
    }

    glfwDestroyWindow(window);
    glfwDestroyWindow(guiWindow);

    // we close and delete the created context
    glfwTerminate();
    return 0;
}


///////////////////////////////////////////
// The function parses the content of the Shader Program, searches for the Subroutine type names, 
// the subroutines implemented for each type, print the names of the subroutines on the terminal, and add the names of 
// the subroutines to the shaders vector, which is used for the shaders swapping
void SetupShader(int program)
{
    int maxSub,maxSubU;
    GLchar name[256]; 
    int len, numCompS;
    
    // global parameters about the Subroutines parameters of the system
    glGetIntegerv(GL_MAX_SUBROUTINES, &maxSub);
    glGetIntegerv(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS, &maxSubU);
    std::cout << "Max Subroutines:" << maxSub << " - Max Subroutine Uniforms:" << maxSubU << std::endl;

    // get the number of Subroutine uniforms (only for the Fragment shader, due to the nature of the exercise)
    // it is possible to add similar calls also for the Vertex shader
    glGetProgramStageiv(program, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINE_UNIFORMS, &countActiveSU);
    
    // print info for every Subroutine uniform
    for (int i = 0; i < countActiveSU; i++) {
        
        // get the name of the Subroutine uniform (in this example, we have only one)
        glGetActiveSubroutineUniformName(program, GL_FRAGMENT_SHADER, i, 256, &len, name);
        // append it to the list of names of subroutine uniforms
        sub_uniforms_names.push_back(name);
        // print index and name of the Subroutine uniform
        std::cout << "Subroutine Uniform: " << i << " - name: " << name << std::endl;

        // get the number of subroutines
        glGetActiveSubroutineUniformiv(program, GL_FRAGMENT_SHADER, i, GL_NUM_COMPATIBLE_SUBROUTINES, &numCompS);
        // save it outside of SetupShader;
        num_compatible_subroutines.push_back(numCompS);
        
        // get the indices of the active subroutines info and write into the array s
        int *s =  new int[numCompS];
        glGetActiveSubroutineUniformiv(program, GL_FRAGMENT_SHADER, i, GL_COMPATIBLE_SUBROUTINES, s);
        compatible_subroutines.push_back(s);
        std::cout << "Compatible Subroutines:" << std::endl;

        // activate the first subroutine for each uniform
        current_subroutines.push_back(compatible_subroutines[i][0]);
        
        // for each index, get the name of the subroutines, print info, and save the name in the shaders vector
        for (int j=0; j < numCompS; ++j) {
            glGetActiveSubroutineName(program, GL_FRAGMENT_SHADER, s[j], 256, &len, name);
            std::cout << "\t" << s[j] << " - " << name << "\n";
            subroutines_names.push_back(name);
        }
        std::cout << std::endl;
        
        // I don't delete s, since its address has been given to the compatible_subroutines vector
        // delete[] s;
        // I will delete all of the arrays at the end of the program
    }
}

//////////////////////////////////////////
// we load the image from disk and we create an OpenGL texture
GLint LoadTexture(const char* path)
{
    GLuint textureImage;
    int w, h, channels;
    unsigned char* image;
    image = stbi_load(path, &w, &h, &channels, STBI_rgb);

    if (image == nullptr)
        std::cout << "Failed to load texture!" << std::endl;

    glGenTextures(1, &textureImage);
    glBindTexture(GL_TEXTURE_2D, textureImage);
    // 3 channels = RGB ; 4 channel = RGBA
    if (channels==3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    else if (channels==4)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    // we set how to consider UVs outside [0,1] range
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // we set the filtering for minification and magnification
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);

    // we free the memory once we have created an OpenGL texture
    stbi_image_free(image);

    // we set the binding to 0 once we have finished
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureImage;

}

//////////////////////////////////////////
// we print on console the name of the currently used shader subroutine
void PrintCurrentShader(int subroutine)
{
    std::cout << "Current shader subroutine: " << subroutines_names[subroutine]  << std::endl;
}


//////////////////////////////////////////
// callback for keyboard events
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    GLuint new_subroutine;

    // if ESC is pressed, we close the application
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // if P is pressed, we start/stop the animated rotation of models
    if(key == GLFW_KEY_P && action == GLFW_PRESS)
        spinning=!spinning;

    // if L is pressed, we activate/deactivate wireframe rendering of models
    if(key == GLFW_KEY_L && action == GLFW_PRESS)
        wireframe=!wireframe;

    // if SPACE is pressed, we activate/deactivate the mouse cursor
    if(key == GLFW_KEY_E && action == GLFW_PRESS)
    {
        if (cursorActive) { // let the application read the mouse again
            glfwSetCursorPosCallback(window, mouse_callback);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetCursorPos(window, lastX, lastY); // forget what the mouse has been doing
        } else { // disable mouse callback
            glfwSetCursorPosCallback(window, NULL);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        cursorActive=!cursorActive;
    }

/*
    // pressing a key number, we change the shader applied to the models
    // if the key is between 1 and 9, we proceed and check if the pressed key corresponds to
    // a valid subroutine
    if((key >= GLFW_KEY_1 && key <= GLFW_KEY_9) && action == GLFW_PRESS)
    {
        // "1" to "9" -> ASCII codes from 49 to 59
        // we subtract 48 (= ASCII CODE of "0") to have integers from 1 to 9
        // we subtract 1 to have indices from 0 to 8
        new_subroutine = (key-'0'-1);
        // if the new index is valid ( = there is a subroutine with that index in the shaders vector),
        // we change the value of the current_subroutine variable
        // NB: we can just check if the new index is in the range between 0 and the size of the shaders vector, 
        // avoiding to use the std::find function on the vector
        if (new_subroutine<subroutines_names.size())
        {
            current_subroutine = new_subroutine;
            PrintCurrentShader(current_subroutine);
        }
    }
*/

    // we keep trace of the pressed keys
    // with this method, we can manage 2 keys pressed at the same time:
    // many I/O managers often consider only 1 key pressed at the time (the first pressed, until it is released)
    // using a boolean array, we can then check and manage all the keys pressed at the same time
    if(action == GLFW_PRESS)
        keys[key] = true;
    else if(action == GLFW_RELEASE)
        keys[key] = false;
}

//////////////////////////////////////////
// If one of the WASD keys is pressed, the camera is moved accordingly (the code is in utils/camera.h)
void apply_camera_movements()
{
    if(keys[GLFW_KEY_W])
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if(keys[GLFW_KEY_S])
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if(keys[GLFW_KEY_A])
        camera.ProcessKeyboard(LEFT, deltaTime);
    if(keys[GLFW_KEY_D])
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if(keys[GLFW_KEY_SPACE])
        camera.ProcessKeyboard(UP, deltaTime);
    if(keys[GLFW_KEY_LEFT_SHIFT])
        camera.ProcessKeyboard(DOWN, deltaTime);
    
}

//////////////////////////////////////////
// callback for mouse events
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
      // we move the camera view following the mouse cursor
      // we calculate the offset of the mouse cursor from the position in the last frame
      // when rendering the first frame, we do not have a "previous state" for the mouse, so we set the previous state equal to the initial values (thus, the offset will be = 0)
    if(firstMouse)
    {
          lastX = xpos;
          lastY = ypos;
          firstMouse = false;
    }

    // offset of mouse cursor position
    GLfloat xoffset = xpos - lastX;
    GLfloat yoffset = lastY - ypos;

    // the new position will be the previous one for the next frame
    lastX = xpos;
    lastY = ypos;

    // we pass the offset to the Camera class instance in order to update the rendering
    camera.ProcessMouseMovement(xoffset, yoffset);

}

// ImGui::RadioButton is commented FIXME in source imgui_widget.cpp
// Issue is it only accepts int*, int as parameters, while we need GLuint to read and write with [GLuint current_subroutine]
// The developer asks to overload function with template type template<T>, having T* and T as parameters, "but I'm not sure how we would expose it.."
// @Overload
bool ImGui::RadioButton(const char* label, GLuint* v, GLuint v_button)
{
    const bool pressed = RadioButton(label, *v == v_button);
    if (pressed)
        *v = v_button;
    return pressed;
}
