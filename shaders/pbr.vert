#version 130

// INPUT: Vertex attributes from your VBO
attribute vec3 aPos;
attribute vec3 aNormal;

// OUTPUT: Variables sent to the fragment shader
varying vec3 WorldPos;
varying vec3 Normal;

// UNIFORMS: Global variables from your C++ application
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat3 u_normalMatrix; // The fix!

void main()
{
    // Calculate the vertex position in world space
    WorldPos = vec3(model * vec4(aPos, 1.0));

    // Transform the normal vector using the pre-calculated normal matrix
    Normal = u_normalMatrix * aNormal;

    // Calculate the final clip-space position of the vertex
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
