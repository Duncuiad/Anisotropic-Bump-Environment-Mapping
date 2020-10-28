/*
13_illumination_models_ML_TX.vert: as 11_illumination_models_ML.vert, but with texturing

N.B. 1) In this example, we consider point lights only. For different kind of lights, the computation must be changed (for example, a directional light is defined by the direction of incident light, so the lightDir is passed as uniform and not calculated in the shader like in this case with a point light).

N.B. 2) 
There are other methods (more efficient) to pass multiple data to the shaders, using for example Uniform Buffer Objects.
With last versions of OpenGL, using structures like the one cited above, it is possible to pass a "dynamic" number of lights
https://www.geeks3d.com/20140704/gpu-buffers-introduction-to-opengl-3-1-uniform-buffers-objects/
https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL (scroll down a bit)
https://hub.packtpub.com/opengl-40-using-uniform-blocks-and-uniform-buffer-objects/

author: Davide Gadia

Real-Time Graphics Programming - a.a. 2019/2020
Master degree in Computer Science
Universita' degli Studi di Milano

*/

#version 410 core

// number of lights in the scene
#define NR_LIGHTS 3

// vertex position in object coordinates
layout (location = 0) in vec3 position;
// UV coordinates
layout (location = 2) in vec2 UV;
// vertex tangent space vectors in object coordinates
layout (location = 1) in vec3 normal;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;
// the numbers used for the location in the layout qualifier are the positions of the vertex attribute
// as defined in the Mesh class

// vectors of lights positions (passed from the application) in world coordinates
uniform vec3 lights[NR_LIGHTS];

// model matrix
uniform mat4 modelMatrix;
// view matrix
uniform mat4 viewMatrix;
// Projection matrix
uniform mat4 projectionMatrix;

// normals transformation matrix (= transpose of the inverse of the model-view matrix)
// used for tangent, bitangent AND normal
uniform mat3 normalMatrix;

// array of light incidence directions (in tangent space)
out vec3 tLightDirs[NR_LIGHTS];

// in the fragment shader, we need to calculate also the reflection vector for each fragment
// to do this, we need to calculate in the vertex shader the view direction (in view coordinates) for each vertex, and to have it interpolated for each fragment by the rasterization stage
out vec3 tViewDirection;

// the output variable for UV coordinates
out vec2 interp_UV;


void main(){

  // Normal, tangent and bitangent (in view coordinates) are used to create the TBN matrix, which maps tangent space to VIEW space
  // I work between tangent and view instead of tangent and object or tangent and world because the TBN matrix, being orthogonal,
  // is far easier to invert, compared to model and view which I would need to invert to send back light incidence directions and view position
  vec3 T = normalize( normalMatrix * tangent );
  vec3 N = normalize( normalMatrix * normal );
  // Apply Gram-Schmidt to T, B, N. It is vital for the TBN matrix to orthogonal
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N,T); // cross(T,N) would invert orientation
  mat3 iTBN = transpose(mat3(T, B, N)); // inverse of TBN matrix

  // vertex position in ModelView coordinate (see the last line for the application of projection)
  // when I need to use coordinates in camera coordinates, I need to split the application of model and view transformations from the projection transformations
  vec4 mvPosition = viewMatrix * modelMatrix * vec4( position, 1.0 );
  
  // view direction, negated to have vector from the vertex to the camera
  tViewDirection = -mvPosition.xyz;
  // view direction is sent to tangent space
  tViewDirection = iTBN * tViewDirection;


  // light incidence directions for all the lights (in view coordinate)
  for (int i=0;i<NR_LIGHTS;i++)
  {
    vec4 lightPos = viewMatrix  * vec4(lights[i], 1.0);;
    tLightDirs[i] = iTBN * (lightPos.xyz - mvPosition.xyz);
  }

  // I assign the values to a variable with "out" qualifier so to use the per-fragment interpolated values in the Fragment shader
  interp_UV = UV;

  // we apply the projection transformation
  gl_Position = projectionMatrix * mvPosition;

}
