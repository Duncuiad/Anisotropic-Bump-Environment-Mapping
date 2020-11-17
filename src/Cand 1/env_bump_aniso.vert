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

// camera position in model coordinates
// passing this as a uniform lets me avoid passing the inverse of the view matrix to the vertex shader, since this is enough to let me calculate the view direction in world coordinates
uniform vec4 wCamera;

// model matrix
uniform mat4 modelMatrix;
// view matrix
uniform mat4 viewMatrix;
// Projection matrix
uniform mat4 projectionMatrix;

// normals transformation matrix (= transpose of the inverse of the model matrix)
// used for tangent, bitangent AND normal
uniform mat3 normalMatrix;

// the fragment shader needs to be given the transformation mapping tangent space to world coordinates
out mat3 wTBNt;

// in the fragment shader, we need to calculate also the reflection vector for each fragment
// to do this, we need to calculate in the vertex shader the view direction (in view coordinates) for each vertex, and to have it interpolated for each fragment by the rasterization stage
out vec3 tViewDirection;

// the output variable for UV coordinates
out vec2 interp_UV;


void main(){

  // Normal, tangent and bitangent (in world coordinates) are used to create the TBN matrix, which maps tangent space to world (model) space
  vec3 T = normalize( normalMatrix * tangent );
  vec3 N = normalize( normalMatrix * normal );
  // Apply Gram-Schmidt to T, B, N. It is vital for the TBN matrix to be orthogonal
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N,T); // cross(T,N) would invert orientation
  wTBNt = mat3(T, B, N); // the TBN matrix; it is an out for the vertex shader
  mat3 iTBN = transpose(wTBNt); // inverse of TBN matrix

  // vertex position in Model coordinate
  vec4 wPosition = modelMatrix * vec4( position, 1.0 );
  
  // view direction, in world coordinates
  tViewDirection = wCamera.xyz - wPosition.xyz;
  // view direction is sent to tangent space
  tViewDirection = iTBN * tViewDirection;

  // I assign the values to a variable with "out" qualifier so to use the per-fragment interpolated values in the fragment shader
  interp_UV = UV;

  // we apply the view and projection transformations
  gl_Position = projectionMatrix * viewMatrix * wPosition;

}
