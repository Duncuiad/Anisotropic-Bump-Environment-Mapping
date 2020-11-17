#ifdef _WIN32
    #define __USE_MINGW_ANSI_STDIO 0
#endif
// Std. Includes
#include <string>
#include <map>

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

// custom max number of subroutine uniforms
#define MY_MAX_SUB_UNIF 32

///////////////////////////////////////////////////////////
// directional shininess(es) for Ashikhmin-Shirley model and sample count for Monte-Carlo integration
GLfloat nU = 20000.0f, nV = 1.0f;
GLuint sampleCount = 5u;

// the paths for the various textures
std::string texturesFolder = "../../textures/";
std::string materialFolder = "hammered_metal/";
std::string materialPath = texturesFolder + materialFolder;

std::string cubeMapsFolder = "arches/";
std::string cubeMapsPath = texturesFolder + cubeMapsFolder;
std::string environmentPath = cubeMapsPath + "environment/";
std::string irradiancePath = cubeMapsPath + "irradiance/";

///////////////////////////////////////////////////////////

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
// a dictionary that matches active subroutine uniform names to their locations
std::map<std::string, int> sub_uniform_location; 
// a dictionary that matches active subroutine names to their indices
std::map<std::string, GLuint> subroutine_index; 

// the name of the subroutines are searched in the shaders, and placed in the shaders vector (to allow shaders swapping)
void SetupShader(int shader_program);

// print on console the name of current shader subroutine
void PrintCurrentShader(int subroutine);

// load image from disk and create an OpenGL texture
GLint LoadTexture(const char* path, bool repeat);
GLint LoadCubeMap(const char* path, const char* format);

// CUSTOM OVERLOAD
// ImGui::RadioButton is commented FIXME in source imgui_widget.cpp
// Issue is it only accepts int*, int as parameters, while we need GLuint to read and write with [GLuint current_subroutine]
// The developer asks to overload function with template type template<T>, having T* and T as parameters, "but I'm not sure how we would expose it.."
// @Overload
namespace ImGui {
    bool RadioButton(const char* label, GLuint* v, GLuint v_button);
    bool SliderInt(const char* label, GLuint* v, GLuint v_min, GLuint v_max, const char* format, ImGuiSliderFlags flags);
}

// return true if the subroutine passed as the second parameter is compatible and currently selected for the subroutine uniform passed as the first parameter
bool currentCompSubIs(int subULocation, GLuint subIndex);
bool currentCompSubIs(std::string subUName, std::string subName);

// we initialize an array of booleans for each keybord key
bool keys[1024];

// we need to store the previous mouse position to calculate the offset with the current frame
GLfloat lastX, lastY;

// when rendering the first frame, we do not have a "previous state" for the mouse, so we need to manage this situation
bool firstMouse = true;
// control the state of the application
GLboolean optionsOverlayActive = GL_FALSE;

// parameters for time calculation (for metrics and animations)
GLfloat setupTime = 0.0f;
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

// Fresnel reflectance at 0 degree (Schlik's approximation)
glm::vec3 F0(0.14, 0.14, 0.14);

// directional shininess(es) for Ashikhmin-Shirley model
GLfloat nX = 1.5f, nY = 100.0f;

// vector for the textures IDs
vector<GLuint> textureID;

// UV repetitions
glm::vec2 repeat = glm::vec2(1.0f, 1.0f);

// height scale for Parallax Occlusion Mapping
GLfloat heightScale = 0.01;

/*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
TODO:
1) Implement textures manager (class Material)
*/

/////////////////// MAIN function ///////////////////////
int main()
{
    // Determine the names of the LUTs based on the shininess values
    std::string strNU = std::to_string(nU);
    strNU.erase(strNU.find_last_not_of('0') + 1, std::string::npos);
    strNU.erase(strNU.find_last_not_of('.') + 1, std::string::npos);
    std::string strNV = std::to_string(nV);
    strNV.erase(strNV.find_last_not_of('0') + 1, std::string::npos);
    strNV.erase(strNV.find_last_not_of('.') + 1, std::string::npos);

    std::string brdfLUTName = "brdfIntegration [" + strNU + "," + strNV + "].png";
    std::string brdfLUTPath = texturesFolder + brdfLUTName;
    std::string hvLUTName = "halfVectorSampling [" + strNU + "," + strNV + "].png";
    std::string hvLUTPath = texturesFolder + hvLUTName;

    // Initialization of OpenGL context using GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    const char* glsl_version = "#version 410";
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    // we set if the window is resizable
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    // Get monitor information
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Anisotropic with Tangent Mapping given by Normal Mapping Perturbation", NULL, NULL);

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
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);


// SCENE SETUP

    // we define the viewport dimensions
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // we enable Z test
    glEnable(GL_DEPTH_TEST);

    //the "clear" color for the frame buffer
    glm::vec4 clear_color = glm::vec4(0.26f, 0.46f, 0.98f, 1.0f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

    // we create the Shader Program used for objects (which presents different subroutines we can switch)
    Shader illumination_shader = Shader("env_bump_aniso.vert", "env_bump_aniso.frag");
    Shader skybox_shader = Shader("skybox.vert", "skybox.frag");
    // we parse the Shader Program to search for the number and names of the subroutines. 
    // the names are placed in the shaders vector
    SetupShader(illumination_shader.Program);
    // we print on console the name of the first subroutine used
    // we load the model(s) (code of Model class is in include/utils/model_v2.h)
    Model cubeModel("../../models/cube.obj");
    Model sphereModel("../../models/sphere.obj");

    // we load the images and store them in a vector
    stbi_set_flip_vertically_on_load(true);    
    textureID.push_back(LoadTexture(brdfLUTPath.c_str(), false));
    textureID.push_back(LoadTexture(hvLUTPath.c_str(), false));
    stbi_set_flip_vertically_on_load(false);
    textureID.push_back(LoadTexture((materialPath + "albedo.jpg").c_str(), true));
    textureID.push_back(LoadTexture((materialPath + "normal.jpg").c_str(), true));
    textureID.push_back(LoadTexture((materialPath + "depth.png").c_str(), true));
    textureID.push_back(LoadTexture((materialPath + "ao.jpg").c_str(), true));
    textureID.push_back(LoadTexture((materialPath + "metallic.jpg").c_str(), true));
    textureID.push_back(LoadTexture((materialPath + "quaternion.png").c_str(), true));
    textureID.push_back(LoadTexture((materialPath + "rotation.png").c_str(), true));    
    textureID.push_back(LoadCubeMap(environmentPath.c_str(), "hdr"));
    textureID.push_back(LoadCubeMap(irradiancePath.c_str(), "hdr"));

    // Projection matrix: FOV angle, aspect ratio, near and far planes
    glm::mat4 projection = glm::perspective(45.0f, (float)width/(float)height, 0.1f, 10000.0f);

    // View matrix: the camera moves, so we just set to indentity now
    glm::mat4 view = glm::mat4(1.0f);

    // Model and Normal transformation matrices for the objects in the scene: we set to identity
    glm::mat4 sphereModelMatrix = glm::mat4(1.0f);
    glm::mat3 sphereNormalMatrix = glm::mat3(1.0f);
    glm::mat4 cubeModelMatrix = glm::mat4(1.0f);
    glm::mat3 cubeNormalMatrix = glm::mat3(1.0f);

    setupTime = glfwGetTime();

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
        if (!optionsOverlayActive)
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

        // activate the illumination shader
        illumination_shader.Use();

        // we determine the position in the Shader Program of the uniform variables
        GLint brdfLUTLocation = glGetUniformLocation(illumination_shader.Program, "brdfLUT");
        GLint hvLUTLocation = glGetUniformLocation(illumination_shader.Program, "halfVector");
        GLint albedoLocation = glGetUniformLocation(illumination_shader.Program, "albedo");
        GLint normalLocation = glGetUniformLocation(illumination_shader.Program, "normMap");
        GLint quaternionLocation = glGetUniformLocation(illumination_shader.Program, "quaternionMap");
        GLint rotationLocation = glGetUniformLocation(illumination_shader.Program, "rotationMap");
        GLint depthLocation = glGetUniformLocation(illumination_shader.Program, "depthMap");
        GLint aoLocation = glGetUniformLocation(illumination_shader.Program, "aoMap");
        GLint metallicLocation = glGetUniformLocation(illumination_shader.Program, "metallicMap");

        // cube maps
        GLint environmentLocation = glGetUniformLocation(illumination_shader.Program, "environmentMap");
        GLint irradianceLocation = glGetUniformLocation(illumination_shader.Program, "irradianceMap");

        GLint sampleCountLocation = glGetUniformLocation(illumination_shader.Program, "sampleCount");
        GLint repeatLocation = glGetUniformLocation(illumination_shader.Program, "repeat");
        GLint f0Location = glGetUniformLocation(illumination_shader.Program, "F0");
        GLint heightScaleLocation = glGetUniformLocation(illumination_shader.Program, "heightScale");
        
        // we assign the value to the uniform variables
        glUniform1ui(sampleCountLocation, sampleCount);
        glUniform2fv(repeatLocation, 1, glm::value_ptr(repeat));
        glUniform3fv(f0Location, 1, glm::value_ptr(F0));
        glUniform1f(heightScaleLocation, heightScale);

        // we pass projection and view matrices to the Shader Program
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "projectionMatrix"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "viewMatrix"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform4fv(glGetUniformLocation(illumination_shader.Program, "wCamera"), 1, glm::value_ptr(glm::vec4(camera.Position, 1.0)));

        /////////////////// OBJECTS ////////////////////////////////////////////////
        // we search inside the Shader Program the name of the subroutine currently selected, and we get the numerical index

        GLuint indices[MY_MAX_SUB_UNIF];
        for (int i = 0; i < countActiveSU; i++) {
            indices[i] = glGetSubroutineIndex(illumination_shader.Program, GL_FRAGMENT_SHADER, subroutines_names[current_subroutines[i]].c_str());
        }
        
        // we activate the desired subroutines using the indices (this is where shaders swapping happens)
        glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, countActiveSU, &indices[0]);

        // Textures and Normal Maps
        // integrated brdf
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID[0]);
        glUniform1i(brdfLUTLocation, 0);

        // mapped half vectors
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureID[1]);
        glUniform1i(hvLUTLocation, 1);

        // albedo
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textureID[2]);
        glUniform1i(albedoLocation, 2);

        // normal map
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, textureID[3]);
        glUniform1i(normalLocation, 3);

        // quaternion map
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, textureID[4]);
        glUniform1i(depthLocation, 4);

        // tangent plane rotation map
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, textureID[5]);
        glUniform1i(aoLocation, 5);

        // depth (1 - height) map
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, textureID[6]);
        glUniform1i(metallicLocation, 6);

        // ambient occlusion map
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, textureID[7]);
        glUniform1i(quaternionLocation, 7);

        // metalness map
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, textureID[8]);
        glUniform1i(rotationLocation, 8);

        // environment cube map
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID[9]);
        glUniform1i(environmentLocation, 9);

        // irradiance cube map
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID[10]);
        glUniform1i(irradianceLocation, 10);

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
        sphereModelMatrix = glm::rotate(sphereModelMatrix, glm::radians(orientationY), glm::vec3(0.0f, 1.0f, 0.0f));
        sphereModelMatrix = glm::scale(sphereModelMatrix, glm::vec3(0.8f, 0.8f, 0.8f));
        // if we cast a mat4 to a mat3, we are automatically considering the upper left 3x3 submatrix
        sphereNormalMatrix = glm::inverseTranspose(glm::mat3(sphereModelMatrix));
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(sphereModelMatrix));
        glUniformMatrix3fv(glGetUniformLocation(illumination_shader.Program, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(sphereNormalMatrix));

        glUniform2fv(repeatLocation, 1, glm::value_ptr(glm::vec2(2.0f, 1.0f)*repeat));
        // we render the sphere
        sphereModel.Draw();
        glUniform2fv(repeatLocation, 1, glm::value_ptr(repeat));

        // SKYBOX

        skybox_shader.Use();
        glUniformMatrix4fv(glGetUniformLocation(skybox_shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(skybox_shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        environmentLocation = glGetUniformLocation(skybox_shader.Program, "environmentMap");

        // environment cube map
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID[9]);
        glUniform1i(environmentLocation, 0);
        glDepthFunc(GL_LEQUAL);
        cubeModel.Draw();
        glDepthFunc(GL_LESS);


        // GUI RENDERING

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (optionsOverlayActive)
        {
            

            ImGui::Begin("Shader Selection");

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

            ImGui::End();

            ImGui::Begin("Parameters");

            ImGui::Text("Shader Parameters");

            ImGui::SliderFloat3("Normal incidence Fresnel reflectance", glm::value_ptr(F0), 0.0001f, 1.0f, "F0 = %.4f", ImGuiSliderFlags_AlwaysClamp);

            ImGui::Separator();

            if (currentCompSubIs("Displacement", "ParallaxMapping"))
            {
                ImGui::SliderFloat("Height Scale", &heightScale, 0.0001, 0.1, "hS = %.4f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::Separator();
            }

            if (currentCompSubIs("Specular", "Specular_Irradiance"))
            {
                ImGui::SliderInt("Sample Count", &sampleCount, 1, 200, "sample count = %.4d", ImGuiSliderFlags_AlwaysClamp);
                ImGui::Separator();
            }

            if (ImGui::TreeNode("Metrics"))
            {
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::Text("Application delta_time %.3f ms/frame (%.1f FPS)", deltaTime * 1000, deltaTime == 0 ? 0 : 1/deltaTime);
                ImGui::TreePop();
            }
            

            ImGui::End();
        }
        else // optionsOverlayActive == false
        {
            ImGui::Begin("Tips");

            ImGui::Text("Controls:");
            ImGui::Text("W, A, S, D to move");
            ImGui::Text("LShift to descend, Space to ascend");
            ImGui::Text("P to toggle animations");
            ImGui::Text("L to toggle wireframe rendering");
            ImGui::Text("E for options");
            ImGui::Text("Esc to close appliation");

            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swapping back and front buffers
        glfwSwapBuffers(window);
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
    int maxSub,maxSubU,activeSub;
    GLchar name[256]; 
    int len, numCompS;
    
    // global parameters about the Subroutines parameters of the system
    glGetIntegerv(GL_MAX_SUBROUTINES, &maxSub);
    glGetIntegerv(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS, &maxSubU);
    glGetProgramStageiv(program, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINES, &activeSub);

    std::cout << "Max Subroutines:" << maxSub << " - Max Subroutine Uniforms:" << maxSubU << " - Active Subroutines:" << activeSub << std::endl;

    // get the number of Subroutine uniforms (only for the Fragment shader, due to the nature of the exercise)
    // it is possible to add similar calls also for the Vertex shader
    glGetProgramStageiv(program, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINE_UNIFORMS, &countActiveSU);

    // initialize vectors to have the right size
    sub_uniforms_names = vector<std::string>(countActiveSU);
    num_compatible_subroutines = vector<int>(countActiveSU);
    compatible_subroutines = vector<int*>(countActiveSU);
    current_subroutines = vector<GLuint>(countActiveSU);
    subroutines_names = vector<std::string>(activeSub);

    /*
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    TODO: SUBSTITUTE ACCESS [loc] WITH at(loc)
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    */

    // print info for every Subroutine uniform
    for (int i = 0; i < countActiveSU; i++) {
        
        // get the name of the Subroutine uniform (in this example, we have only one)
        glGetActiveSubroutineUniformName(program, GL_FRAGMENT_SHADER, i, 256, &len, name);
        int loc = glGetSubroutineUniformLocation(program, GL_FRAGMENT_SHADER, name);
        // append it to the list of names of subroutine uniforms
        sub_uniforms_names.at(loc) = name;
        sub_uniform_location.emplace(name, loc);
        // print index and name of the Subroutine uniform
        std::cout << "Subroutine Uniform, index: " << i << " - location: " << loc << " - name: " << name << std::endl;

        // get the number of subroutines
        glGetActiveSubroutineUniformiv(program, GL_FRAGMENT_SHADER, i, GL_NUM_COMPATIBLE_SUBROUTINES, &numCompS);
        // save it outside of SetupShader;
        num_compatible_subroutines.at(loc) = numCompS;
        
        // get the indices of the active subroutines info and write into the array s
        int *s =  new int[numCompS];
        glGetActiveSubroutineUniformiv(program, GL_FRAGMENT_SHADER, i, GL_COMPATIBLE_SUBROUTINES, s);
        compatible_subroutines.at(loc) = s;
        std::cout << "Compatible Subroutines:" << std::endl;

        // activate the first subroutine for each uniform
        current_subroutines.at(loc) = compatible_subroutines.at(loc)[0];
        
        // for each index, get the name of the subroutines, print info, and save the name in the shaders vector
        for (int j=0; j < numCompS; ++j) {
            glGetActiveSubroutineName(program, GL_FRAGMENT_SHADER, s[j], 256, &len, name);
            std::cout << "\t" << s[j] << " - " << name << "\n";
            subroutines_names.at(s[j]) = name;
            subroutine_index.emplace(name, s[j]);
        }
        std::cout << std::endl;
        
        // I don't delete s, since its address has been given to the compatible_subroutines vector
        // delete[] s;
        // I will delete all of the arrays at the end of the program
    }
}

//////////////////////////////////////////
// we load the image from disk and we create an OpenGL texture
GLint LoadTexture(const char* path, bool repeat)
{

    GLuint textureImage;
    int w, h, channels;
    unsigned char* image;
    image = stbi_load(path, &w, &h, &channels, STBI_default);

    if (image == nullptr)
        std::cout << "Failed to load texture!" << std::endl;

    glGenTextures(1, &textureImage);
    glBindTexture(GL_TEXTURE_2D, textureImage);
    // 3 channels = RGB ; 4 channel = RGBA
    if (channels==1)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, image);
    else if (channels==2)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, w, h, 0, GL_R16, GL_UNSIGNED_BYTE, image);
    else if (channels==3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    else if (channels==4)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    else
    {
        std::cout << "Error loading the texture: number of channels must be 1, 2, 3 or 4" << std::endl;
        std::cout << "Image: " << path << ", number of channels: " << channels << std::endl;
    }
    
    glGenerateMipmap(GL_TEXTURE_2D);
    // we set how to consider UVs outside [0,1] range
    if (repeat) 
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    

    // we set the filtering for minification and magnification
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // we free the memory once we have created an OpenGL texture
    stbi_image_free(image);

    // we set the binding to 0 once we have finished
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureImage;

}

// we load the image from disk and we create an OpenGL texture
GLint LoadCubeMap(const char* path, const char* format)
{
    int width, height, nrChannels;
    unsigned char *data;  
    std::string sPath(path);
    std::string sFormat(format);
    std::vector<std::string> textures_faces = {"right", "left", "up", "down", "back", "front"};
    GLuint textureImage;
    glGenTextures(1, &textureImage);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureImage);

    for(unsigned int i = 0; i < textures_faces.size(); i++)
    {
        data = stbi_load((sPath + textures_faces[i] + "." + sFormat).c_str(), &width, &height, &nrChannels, 0);
        if (data == nullptr)
            std::cout << "Failed to load texture!" << std::endl;
        glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
    
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    // we set how to consider UVs outside [0,1] range
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);  

    // we free the memory once we have created an OpenGL texture
    stbi_image_free(data);

    // we set the binding to 0 once we have finished
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

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
        if (optionsOverlayActive) { // let the application read the mouse again
            glfwSetCursorPosCallback(window, mouse_callback);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetCursorPos(window, lastX, lastY); // forget what the mouse has been doing
        } else { // disable mouse callback
            glfwSetCursorPosCallback(window, NULL);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        optionsOverlayActive=!optionsOverlayActive;
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

// Same as above
// @Overload
bool ImGui::SliderInt(const char* label, GLuint* v, GLuint v_min, GLuint v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalar(label, ImGuiDataType_U32, v, &v_min, &v_max, format, flags);
}

// this function takes the value "perturbedNormal" of the normal map in a texel and returns the quaternion that rotates N = (0.0, 0.0, 1.0) to perturbedNormal (in tangent space coordinates)
// it returns a vec3 because the quaternion q = a + bi + cj + dk we calculate always has d == 0
// to speed up calculations, I omit the last coordinate
// IMPORTANT: the argument is supposed normalized
glm::vec3 RotationQuaternion(glm::vec3 perturbedNormal)
{
    float a = sqrt((perturbedNormal.z + 1.0)/2.0);
    float b = perturbedNormal.y / (2*a);
    float c = -perturbedNormal.x / (2*a);
    // float d = 0;
    return glm::vec3(a, b, c);
}

bool currentCompSubIs(int subULocation, GLuint subIndex)
{
    return current_subroutines[subULocation] == subIndex;
}

bool currentCompSubIs(std::string subUName, std::string subName)
{
    return currentCompSubIs(sub_uniform_location.at(subUName), subroutine_index.at(subName));
}