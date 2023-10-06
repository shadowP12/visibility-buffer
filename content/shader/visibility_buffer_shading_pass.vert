#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 0) out vec2 out_screen_pos;

void main()
{
    gl_Position = vec4(in_position, 1.0);
    out_screen_pos = in_position.xy;
}