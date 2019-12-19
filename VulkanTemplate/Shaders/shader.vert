#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

/*
// dvec3 ���� 64��Ʈ vectors �� location�� �ϳ� �� ����� �� ����.
layout(location = 0) in dvec3 inPosition;   // dvec3 is 64 bit vector.
layout(location = 2) in vec3 inColor;
*/

layout(location = 0) out vec3 fragColor;

void main() 
{
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
