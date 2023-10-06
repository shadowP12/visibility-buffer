#version 450

// Based on The Forge's Visibility Buffer implementation.
// <https://github.com/ConfettiFX/The-Forge/blob/v1.45/Examples_3/Visibility_Buffer/src/Shaders/Vulkan/visibilityBuffer_shade.frag>

#extension GL_GOOGLE_include_directive : enable

#include "shader_defs.glsl"

layout(location = 0) in vec2 in_screen_pos;
layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform texture2D vb_tex;
layout(binding = 1) uniform sampler vb_sampler;

struct Vertex
{
    float x, y, z;
};

struct Normal
{
    float x, y, z;
};

struct UV
{
    float u, v;
};

layout(std430, binding = 2) restrict readonly buffer VertexDataBufferBlock
{
    Vertex data[];
} vertex_data_buffer;

layout(std430, binding = 3) restrict readonly buffer NormalDataBufferBlock
{
    Normal data[];
} normal_data_buffer;

layout(std430, binding = 4) restrict readonly buffer UVDataBufferBlock
{
    UV data[];
} uv_data_buffer;

layout(std430, binding = 5) restrict readonly buffer FilteredIndicesBufferBlock
{
    uint data[];
} filtered_indices_buffer;

layout(std430, binding = 6) restrict buffer DrawCommandBufferBlock
{
    DrawIndexedIndirectCommand data[];
} draw_command_buffer;

layout(std140, binding = 7) uniform ViewBuffer
{
    mat4 view_matrix;
    mat4 proj_matrix;
    mat4 pad0;
    mat4 pad1;
} view_buffer;

struct Derivatives
{
    vec3 ddx;
    vec3 ddy;
};

Derivatives ComputePartialDerivatives(vec2 v[3])
{
    Derivatives derivative;
    float d = 1.0 / determinant(mat2(v[2] - v[1], v[0] - v[1]));
    derivative.ddx = vec3(v[1].y - v[2].y, v[2].y - v[0].y, v[0].y - v[1].y) * d;
    derivative.ddy = vec3(v[2].x - v[1].x, v[0].x - v[2].x, v[1].x - v[0].x) * d;
    return derivative;
}

vec3 InterpolateAttribute(mat3 attributes, vec3 ddx, vec3 ddy, vec2 d)
{
    vec3 attribute_x = attributes * ddx;
    vec3 attribute_y = attributes * ddy;
    vec3 attribute_s = attributes[0];

    return (attribute_s + d.x * attribute_x + d.y * attribute_y);
}

float InterpolateAttribute(vec3 attributes, vec3 ddx, vec3 ddy, vec2 d)
{
    float attribute_x = dot(attributes, ddx);
    float attribute_y = dot(attributes, ddy);
    float attribute_s = attributes[0];

    return (attribute_s + d.x * attribute_x + d.y * attribute_y);
}

void main()
{
    vec4 vis_raw = texelFetch(sampler2D(vb_tex, vb_sampler), ivec2(gl_FragCoord.xy), 0);
    uint draw_id_tri_id = packUnorm4x8(vis_raw);

    if (draw_id_tri_id != ~0)
    {
        uint draw_id = (draw_id_tri_id >> 23) & uint(0x000000FF);
        uint triangle_id = (draw_id_tri_id & uint(0x007FFFFF));

        uint start_index = draw_command_buffer.data[draw_id].first_index;

        uint tri_idx0 = (triangle_id * 3 + 0) + start_index;
        uint tri_idx1 = (triangle_id * 3 + 1) + start_index;
        uint tri_idx2 = (triangle_id * 3 + 2) + start_index;

        uint index0 = filtered_indices_buffer.data[tri_idx0];
        uint index1 = filtered_indices_buffer.data[tri_idx1];
        uint index2 = filtered_indices_buffer.data[tri_idx2];

        vec3 v0 = vec3(vertex_data_buffer.data[index0].x, vertex_data_buffer.data[index0].y, vertex_data_buffer.data[index0].z);
        vec3 v1 = vec3(vertex_data_buffer.data[index1].x, vertex_data_buffer.data[index1].y, vertex_data_buffer.data[index1].z);
        vec3 v2 = vec3(vertex_data_buffer.data[index2].x, vertex_data_buffer.data[index2].y, vertex_data_buffer.data[index2].z);

        mat4 mvp = view_buffer.proj_matrix * view_buffer.view_matrix;
        mat4 inv_vp = inverse(mvp);

        vec4 pos0 = mvp * vec4(v0, 1);
        vec4 pos1 = mvp * vec4(v1, 1);
        vec4 pos2 = mvp * vec4(v2, 1);
        vec3 one_over_w = 1.0 / vec3(pos0.w, pos1.w, pos2.w);

        pos0 *= one_over_w[0];
        pos1 *= one_over_w[1];
        pos2 *= one_over_w[2];

        vec2 pos_scr[3] = { pos0.xy, pos1.xy, pos2.xy };

        Derivatives derivatives = ComputePartialDerivatives(pos_scr);

        vec2 d = in_screen_pos + -pos_scr[0];

        float w = 1.0 / InterpolateAttribute(one_over_w, derivatives.ddx, derivatives.ddy, d);

        float z = w * view_buffer.proj_matrix[2][2] + view_buffer.proj_matrix[3][2];

        vec3 position = (inv_vp * vec4(in_screen_pos * w, z, w)).xyz;

        mat3x3 normals =
        {
            vec3(normal_data_buffer.data[index0].x, normal_data_buffer.data[index0].y, normal_data_buffer.data[index0].z) * one_over_w[0],
            vec3(normal_data_buffer.data[index1].x, normal_data_buffer.data[index1].y, normal_data_buffer.data[index1].z) * one_over_w[1],
            vec3(normal_data_buffer.data[index2].x, normal_data_buffer.data[index2].y, normal_data_buffer.data[index2].z) * one_over_w[2]
        };

        vec3 normal = normalize(InterpolateAttribute(normals, derivatives.ddx, derivatives.ddy, d));

        // Shading
        vec3 sun_direction = normalize(vec3(0.0, -1.0, -1.0));
        out_color = vec4(max(dot(normal, -sun_direction), 0.0) * vec3(0.6) + vec3(0.1), 1.0);
    }
    else
    {
        out_color = vec4(1.0, 1.0, 1.0, 0.0);
    }
}