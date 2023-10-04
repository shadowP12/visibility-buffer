#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shader_draw_parameters : enable

layout(location = 0) in vec3 in_position;
layout(location = 0) out uint out_draw_id;

layout(std140, binding = 0) uniform ViewBuffer
{
    mat4 view_matrix;
    mat4 proj_matrix;
    mat4 pad0;
    mat4 pad1;
} view_buffer;

void main()
{
    uint draw_id = gl_DrawIDARB;
    gl_Position = view_buffer.proj_matrix * view_buffer.view_matrix * vec4(in_position, 1);
    out_draw_id = draw_id;
}