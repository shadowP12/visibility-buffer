#include "rsg.h"

EzBuffer RSG::quad_buffer = VK_NULL_HANDLE;
EzBuffer RSG::cube_buffer = VK_NULL_HANDLE;

void create_quad_buffer()
{
    static float quad_vertices[] = {
        // positions        // texcoords
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f
    };

    EzBufferDesc buffer_desc = {};
    buffer_desc.size = sizeof(quad_vertices);
    buffer_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_desc.memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ez_create_buffer(buffer_desc, RSG::quad_buffer);

    VkBufferMemoryBarrier2 barrier = ez_buffer_barrier(RSG::quad_buffer, EZ_RESOURCE_STATE_COPY_DEST);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);
    ez_update_buffer(RSG::quad_buffer, sizeof(quad_vertices), 0, quad_vertices);
    barrier = ez_buffer_barrier(RSG::quad_buffer, EZ_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);
}

void create_cube_buffer()
{
    static float cube_vertices[] = {
        // positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f,  1.0f
    };

    EzBufferDesc buffer_desc = {};
    buffer_desc.size = sizeof(cube_vertices);
    buffer_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_desc.memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ez_create_buffer(buffer_desc, RSG::cube_buffer);

    VkBufferMemoryBarrier2 barrier = ez_buffer_barrier(RSG::cube_buffer, EZ_RESOURCE_STATE_COPY_DEST);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);
    ez_update_buffer(RSG::cube_buffer, sizeof(cube_vertices), 0, cube_vertices);
    barrier = ez_buffer_barrier(RSG::cube_buffer, EZ_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);
}

void init_rsg()
{
    create_quad_buffer();
    create_cube_buffer();
}

void uninit_rsg()
{
    ez_destroy_buffer(RSG::quad_buffer);
    ez_destroy_buffer(RSG::cube_buffer);
}