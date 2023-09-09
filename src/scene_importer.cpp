#include "scene_importer.h"
#include "scene.h"
#include <core/path.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <map>
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <rhi/ez_vulkan.h>

glm::mat4 get_local_matrix(cgltf_node* node)
{
    glm::vec3 translation = glm::vec3(0.0f);
    if (node->has_translation)
    {
        translation.x = node->translation[0];
        translation.y = node->translation[1];
        translation.z = node->translation[2];
    }

    glm::quat rotation = glm::quat(1, 0, 0, 0);
    if (node->has_rotation)
    {
        rotation.x = node->rotation[0];
        rotation.y = node->rotation[1];
        rotation.z = node->rotation[2];
        rotation.w = node->rotation[3];
    }

    glm::vec3 scale = glm::vec3(1.0f);
    if (node->has_scale)
    {
        scale.x = node->scale[0];
        scale.y = node->scale[1];
        scale.z = node->scale[2];
    }

    glm::mat4 r, t, s;
    r = glm::toMat4(rotation);
    t = glm::translate(glm::mat4(1.0), translation);
    s = glm::scale(glm::mat4(1.0), scale);
    return t * r * s;
}

glm::mat4 get_world_matrix(cgltf_node* node)
{
    cgltf_node* cur_node = node;
    glm::mat4 out = get_local_matrix(cur_node);

    while (cur_node->parent != nullptr)
    {
        cur_node = node->parent;
        out = get_local_matrix(cur_node) * out;
    }
    return out;
}

cgltf_attribute* get_gltf_attribute(cgltf_primitive* primitive, cgltf_attribute_type type)
{
    for (int i = 0; i < primitive->attributes_count; i++)
    {
        cgltf_attribute* att = &primitive->attributes[i];
        if(att->type == type)
            return att;
    }
    return nullptr;
}

VkPrimitiveTopology get_topology(cgltf_primitive_type primitive_type)
{
    switch (primitive_type)
    {
        case cgltf_primitive_type_points:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case cgltf_primitive_type_lines:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case cgltf_primitive_type_line_strip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case cgltf_primitive_type_triangles:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case cgltf_primitive_type_triangle_strip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case cgltf_primitive_type_triangle_fan:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default:
            assert(0);
            break;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

void combind_vertex_data(void* dst, void* src, uint32_t vertex_count, uint32_t attribute_size, uint32_t offset, uint32_t stride, uint32_t size)
{
    uint8_t* dst_data = (uint8_t*)dst + offset;
    uint8_t* src_data = (uint8_t*)src;
    for (uint32_t i = 0; i < vertex_count; i++)
    {
        memcpy(dst_data, src_data, attribute_size);
        dst_data += stride;
        src_data += attribute_size;
    }
}

EzBuffer create_vertex_buffer(void* data, uint32_t data_size)
{
    EzBuffer buffer;
    EzBufferDesc buffer_desc = {};
    buffer_desc.size = data_size;
    buffer_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_desc.memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ez_create_buffer(buffer_desc, buffer);

    VkBufferMemoryBarrier2 barrier = ez_buffer_barrier(buffer,VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);

    ez_update_buffer(buffer, data_size, 0, data);

    barrier = ez_buffer_barrier(buffer,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);

    return buffer;
}

EzBuffer create_index_buffer(void* data, uint32_t data_size)
{
    EzBuffer buffer;
    EzBufferDesc buffer_desc = {};
    buffer_desc.size = data_size;
    buffer_desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    buffer_desc.memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ez_create_buffer(buffer_desc, buffer);

    VkBufferMemoryBarrier2 barrier = ez_buffer_barrier(buffer,
                                                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                       VK_ACCESS_TRANSFER_WRITE_BIT);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);

    ez_update_buffer(buffer, data_size, 0, data);

    barrier = ez_buffer_barrier(buffer,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDEX_READ_BIT);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);

    return buffer;
}

Scene* load_scene(const std::string& file_path)
{
    std::string fix_path = Path::fix_path(file_path);
    Scene* scene = new Scene();

    cgltf_options options = {static_cast<cgltf_file_type>(0)};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, fix_path.c_str(), &data) != cgltf_result_success)
    {
        cgltf_free(data);
        return nullptr;
    }

    if (cgltf_load_buffers(&options, data, fix_path.c_str()) != cgltf_result_success)
    {
        cgltf_free(data);
        return nullptr;
    }

    if (cgltf_validate(data) != cgltf_result_success)
    {
        cgltf_free(data);
        return nullptr;
    }

    // Load meshes
    std::map<cgltf_mesh*, Mesh*> mesh_helper;
    for (size_t i = 0; i < data->meshes_count; ++i)
    {
        Mesh* mesh = new Mesh();
        scene->meshes.push_back(mesh);
        cgltf_mesh* cmesh = &data->meshes[i];
        mesh_helper[cmesh] = mesh;

        for (size_t j = 0; j < cmesh->primitives_count; j++)
        {
            Primitive* primitive = new Primitive();
            mesh->primitives.push_back(primitive);
            scene->primitives.push_back(primitive);

            cgltf_primitive* cprimitive = &cmesh->primitives[j];
            primitive->topology = get_topology(cprimitive->type);

            // Vertices
            uint32_t vertex_count = 0;
            uint32_t vertex_buffer_size = 0;
            uint32_t vertex_buffer_offset = 0;
            uint32_t position_offset = -1;
            uint32_t uv_offset = -1;
            uint32_t normal_offset = -1;
            uint32_t position_attribute_size = 0;
            uint32_t uv_attribute_size = 0;
            uint32_t normal_attribute_size = 0;
            uint32_t position_data_size = 0;
            uint32_t uv_data_size = 0;
            uint32_t normal_data_size = 0;

            cgltf_attribute* position_attribute = get_gltf_attribute(cprimitive, cgltf_attribute_type_position);
            cgltf_accessor* position_accessor = position_attribute->data;
            cgltf_buffer_view* position_view = position_accessor->buffer_view;
            uint8_t* position_data = (uint8_t*)(position_view->buffer->data) + position_accessor->offset + position_view->offset;
            vertex_count = (uint32_t)position_accessor->count;
            position_offset = vertex_buffer_offset;
            position_attribute_size = sizeof(glm::vec3);
            position_data_size = position_attribute_size * vertex_count;
            vertex_buffer_offset += position_attribute_size;
            vertex_buffer_size += position_data_size;
            float* minp = &position_accessor->min[0];
            float* maxp = &position_accessor->max[0];

            cgltf_attribute* normal_attribute = get_gltf_attribute(cprimitive, cgltf_attribute_type_normal);
            cgltf_accessor* normal_accessor = normal_attribute->data;
            cgltf_buffer_view* normal_view = normal_accessor->buffer_view;
            uint8_t* normal_data = (uint8_t*)(normal_view->buffer->data) + normal_accessor->offset + normal_view->offset;
            normal_offset = vertex_buffer_offset;
            normal_attribute_size = sizeof(glm::vec3);
            normal_data_size = normal_attribute_size * vertex_count;
            vertex_buffer_offset += normal_attribute_size;
            vertex_buffer_size += normal_data_size;

            cgltf_attribute* texcoord_attribute = get_gltf_attribute(cprimitive, cgltf_attribute_type_texcoord);
            cgltf_accessor* texcoord_accessor = texcoord_attribute->data;
            cgltf_buffer_view* texcoord_view = texcoord_accessor->buffer_view;
            uint8_t* uv_data = (uint8_t*)(texcoord_view->buffer->data) + texcoord_accessor->offset + texcoord_view->offset;
            uv_offset = vertex_buffer_offset;
            uv_attribute_size = sizeof(glm::vec2);
            uv_data_size = uv_attribute_size * vertex_count;
            vertex_buffer_offset += uv_attribute_size;
            vertex_buffer_size += uv_data_size;

            uint8_t* vertex_data = (uint8_t*)malloc(sizeof(uint8_t) * vertex_buffer_size);
            combind_vertex_data(vertex_data, position_data, vertex_count, position_attribute_size, position_offset, vertex_buffer_offset, position_data_size);
            combind_vertex_data(vertex_data, normal_data, vertex_count, normal_attribute_size, normal_offset, vertex_buffer_offset, normal_data_size);
            combind_vertex_data(vertex_data, uv_data, vertex_count, uv_attribute_size, uv_offset, vertex_buffer_offset, uv_data_size);
            primitive->vertex_count = vertex_count;
            primitive->vertex_buffer = create_vertex_buffer(vertex_data, sizeof(uint8_t) * vertex_buffer_size);
            free(vertex_data);

            // Indices
            cgltf_accessor* index_accessor = cprimitive->indices;
            cgltf_buffer_view* index_buffer_view = index_accessor->buffer_view;
            cgltf_buffer* index_buffer = index_buffer_view->buffer;
            uint8_t* index_data = (uint8_t*)index_buffer->data + index_accessor->offset + index_buffer_view->offset;
            uint32_t index_count = (uint32_t)index_accessor->count;
            uint32_t index_data_size = 0;
            if (index_accessor->component_type == cgltf_component_type_r_16u)
            {
                primitive->index_type = VK_INDEX_TYPE_UINT16;
                index_data_size = index_count * sizeof(uint16_t);
            }
            else if (index_accessor->component_type == cgltf_component_type_r_32u)
            {
                primitive->index_type = VK_INDEX_TYPE_UINT32;
                index_data_size = index_count * sizeof(uint32_t);
            }
            primitive->index_count = index_count;
            primitive->index_buffer = create_index_buffer(index_data, index_data_size);

            // Bounds
            primitive->bounds.merge(glm::vec3(minp[0], minp[1], minp[2]));
            primitive->bounds.merge(glm::vec3(maxp[0], maxp[1], maxp[2]));
            primitive->bounds.grow(0.02f);
        }
    }

    // Load nodes
    for (size_t i = 0; i < data->nodes_count; ++i)
    {
        Node* node = new Node();
        scene->nodes.push_back(node);
        cgltf_node* cnode = &data->nodes[i];

        node->name = cnode->name;

        node->transform = get_world_matrix(cnode);

        if (cnode->mesh)
            node->mesh = mesh_helper[cnode->mesh];
    }

    cgltf_free(data);
    return scene;
}