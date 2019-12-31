#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 Model;
    mat4 View;
    mat4 Proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

/*
// dvec3 ���� 64��Ʈ vectors �� location�� �ϳ� �� ����� �� ����.
layout(location = 0) in dvec3 inPosition;   // dvec3 is 64 bit vector.
layout(location = 2) in vec3 inColor;
*/

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() 
{
    gl_Position = ubo.Proj * ubo.View * ubo.Model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
