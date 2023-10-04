#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in flat uint in_draw_id;
layout(location = 0) out vec4 out_color;

uint CalculateOutputID(uint draw_id, uint primitive_id)
{
    uint id = ((draw_id << 23) & uint(0x7F800000)) | (primitive_id & uint(0x007FFFFF));
    return id;
}

void main()
{
    out_color = unpackUnorm4x8(CalculateOutputID(in_draw_id, gl_PrimitiveID));
}