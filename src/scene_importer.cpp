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

EzBuffer create_rw_buffer(void* data, uint32_t data_size, VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
{
    EzBuffer buffer;
    EzBufferDesc buffer_desc = {};
    buffer_desc.size = data_size;
    buffer_desc.usage = usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_desc.memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ez_create_buffer(buffer_desc, buffer);

    VkBufferMemoryBarrier2 barrier;
    if (data)
    {
        barrier = ez_buffer_barrier(buffer, EZ_RESOURCE_STATE_COPY_DEST);
        ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);
        ez_update_buffer(buffer, data_size, 0, data);
    }

    EzResourceState flag = EZ_RESOURCE_STATE_UNDEFINED;
    if ((usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0)
        flag |= EZ_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if ((usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0)
        flag |= EZ_RESOURCE_STATE_INDEX_BUFFER;
    if ((usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) != 0)
        flag |= EZ_RESOURCE_STATE_INDIRECT_ARGUMENT;
    barrier = ez_buffer_barrier(buffer, EZ_RESOURCE_STATE_SHADER_RESOURCE | EZ_RESOURCE_STATE_UNORDERED_ACCESS | flag);
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

    std::vector<glm::mat4> transforms;
    std::vector<MeshConstants> mesh_constants_list;
    std::vector<float> total_position_data;
    std::vector<float> total_normal_data;
    std::vector<float> total_uv_data;
    std::vector<uint32_t> total_index_data;
    for (size_t i = 0; i < data->nodes_count; ++i)
    {
        cgltf_node* cnode = &data->nodes[i];

        if (!cnode->mesh)
            continue;

        glm::mat4 transform = get_world_matrix(cnode);
        cgltf_mesh* cmesh = &data->meshes[i];
        for (size_t j = 0; j < cmesh->primitives_count; j++)
        {
            cgltf_primitive* cprimitive = &cmesh->primitives[j];

            cgltf_attribute* position_attribute = get_gltf_attribute(cprimitive, cgltf_attribute_type_position);
            cgltf_accessor* position_accessor = position_attribute->data;
            cgltf_buffer_view* position_view = position_accessor->buffer_view;
            uint32_t vertex_count = (uint32_t)position_accessor->count;
            float* position_data = (float*)((uint8_t*)(position_view->buffer->data) + position_accessor->offset + position_view->offset);
            total_position_data.insert(total_position_data.end(), position_data, position_data + vertex_count * 3);

            cgltf_attribute* normal_attribute = get_gltf_attribute(cprimitive, cgltf_attribute_type_normal);
            cgltf_accessor* normal_accessor = normal_attribute->data;
            cgltf_buffer_view* normal_view = normal_accessor->buffer_view;
            float* normal_data = (float*)((uint8_t*)(normal_view->buffer->data) + normal_accessor->offset + normal_view->offset);
            total_normal_data.insert(total_normal_data.end(), normal_data, normal_data + vertex_count * 3);

            cgltf_attribute* texcoord_attribute = get_gltf_attribute(cprimitive, cgltf_attribute_type_texcoord);
            cgltf_accessor* texcoord_accessor = texcoord_attribute->data;
            cgltf_buffer_view* texcoord_view = texcoord_accessor->buffer_view;
            float* uv_data = (float*)((uint8_t*)(texcoord_view->buffer->data) + texcoord_accessor->offset + texcoord_view->offset);
            total_uv_data.insert(total_uv_data.end(), uv_data, uv_data + vertex_count * 2);

            cgltf_accessor* index_accessor = cprimitive->indices;
            cgltf_buffer_view* index_buffer_view = index_accessor->buffer_view;
            cgltf_buffer* index_buffer = index_buffer_view->buffer;
            uint32_t index_count = (uint32_t)index_accessor->count;
            uint32_t* index_data = nullptr;
            uint32_t* index_u32_data = nullptr;
            uint16_t* index_u16_data = nullptr;
            if (index_accessor->component_type == cgltf_component_type_r_16u)
            {
                index_u32_data = new uint32_t[index_count];
                index_u16_data = (uint16_t*)((uint8_t*)index_buffer->data + index_accessor->offset + index_buffer_view->offset);
                for (int k = 0; k < index_count; ++k)
                {
                    index_u32_data[k] = (uint32_t)index_u16_data[k];
                }
                index_data = index_u32_data;
            }
            else if (index_accessor->component_type == cgltf_component_type_r_32u)
            {
                index_data = (uint32_t*)((uint8_t*)index_buffer->data + index_accessor->offset + index_buffer_view->offset);
            }

            total_index_data.insert(total_index_data.end(), index_data, index_data + index_count);

            // Cluster
            // Based on "AMD GeometryFX" - https://github.com/GPUOpen-Effects/GeometryFX
            uint32_t triangle_count = index_count / 3;
            uint32_t cluster_count = (triangle_count + CLUSTER_SIZE - 1) / CLUSTER_SIZE;
            Mesh mesh;
            for (size_t k = 0; k < cluster_count; ++k)
            {
                uint32_t start = k * CLUSTER_SIZE;
                uint32_t end = glm::min(start + CLUSTER_SIZE, triangle_count);

                glm::vec3 cone_axis = glm::vec3(0.0f, 0.0f, 0.0f);
                BoundingBox bounds;
                for (size_t triangle_index = start; triangle_index < end; ++triangle_index)
                {
                    int idx0 = (int)index_data[triangle_index * 3 + 0];
                    int idx1 = (int)index_data[triangle_index * 3 + 1];
                    int idx2 = (int)index_data[triangle_index * 3 + 2];
                    glm::vec3 v0(position_data[idx0 * 3], position_data[idx0 * 3 + 1], position_data[idx0 * 3 + 2]);
                    glm::vec3 v1(position_data[idx1 * 3], position_data[idx1 * 3 + 1], position_data[idx1 * 3 + 2]);
                    glm::vec3 v2(position_data[idx2 * 3], position_data[idx2 * 3 + 1], position_data[idx2 * 3 + 2]);
                    bounds.merge(v0);
                    bounds.merge(v1);
                    bounds.merge(v2);
                    glm::vec3 triangle_normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                    cone_axis = cone_axis - triangle_normal;
                }

                float cone_opening = 1;
                bool valid_cluster = true;
                if (cone_axis == glm::vec3(0.0, 0.0, 0.0))
                    valid_cluster = false;
                cone_axis = glm::normalize(cone_axis);
                glm::vec3 center = bounds.get_center();

                float t = NEG_INF;
                for (size_t triangle_index = start; triangle_index < end; ++triangle_index)
                {
                    int idx0 = (int)index_data[triangle_index * 3 + 0];
                    int idx1 = (int)index_data[triangle_index * 3 + 1];
                    int idx2 = (int)index_data[triangle_index * 3 + 2];
                    glm::vec3 v0(position_data[idx0 * 3], position_data[idx0 * 3 + 1], position_data[idx0 * 3 + 2]);
                    glm::vec3 v1(position_data[idx1 * 3], position_data[idx1 * 3 + 1], position_data[idx1 * 3 + 2]);
                    glm::vec3 v2(position_data[idx2 * 3], position_data[idx2 * 3 + 1], position_data[idx2 * 3 + 2]);
                    glm::vec3 triangle_normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

                    const float directional_part = glm::dot(cone_axis, -triangle_normal);

                    if (directional_part <= 0)
                    {
                        // No solution for this cluster - at least two triangles are facing each other
                        valid_cluster = false;
                        break;
                    }

                    // We need to intersect the plane with our cone ray which is center + t * coneAxis, and find the max
                    // t along the cone ray (which points into the empty space) See: https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
                    const float td = dot(center - v0, triangle_normal) / -directional_part;

                    t = glm::max(t, td);

                    cone_opening = glm::min(cone_opening, directional_part);
                }

                Cluster cluster{};
                cluster.aabb_max = bounds.bb_max;
                cluster.aabb_min = bounds.bb_min;
                cluster.cone_axis = cone_axis;
                cluster.cone_center = center + cone_axis * t;
                // cos (PI/2 - acos (coneOpening))
                cluster.cone_angle_cosine = glm::sqrt(1 - cone_opening * cone_opening);

                // AMD_GEOMETRY_FX_ENABLE_CLUSTER_CENTER_SAFETY_CHECK
                float aabb_size = glm::length(bounds.get_size());
                float cone_center_to_center_distance = glm::length(cluster.cone_center - center);
                if (cone_center_to_center_distance > (16 * aabb_size))
                    valid_cluster = false;
                cluster.valid = valid_cluster;

                ClusterCompact compact{};
                compact.cluster_start = start;
                compact.triangle_count = end - start;

                mesh.clusters.push_back(cluster);
                mesh.compacts.push_back(compact);
            }
            scene->meshs.push_back(mesh);

            MeshConstants mesh_constants{};
            mesh_constants.face_count = triangle_count;
            mesh_constants.index_offset = scene->index_count;
            mesh_constants_list.push_back(mesh_constants);

            VkDrawIndexedIndirectCommand draw_command;
            draw_command.firstInstance = 0;
            draw_command.instanceCount = 1;
            draw_command.firstIndex = scene->index_count;
            draw_command.indexCount = index_count;
            draw_command.vertexOffset = 0;
            scene->draw_commands.push_back(draw_command);
            transforms.push_back(transform);

            scene->vertex_count += vertex_count;
            scene->index_count += index_count;

            if (index_u32_data)
                delete[] index_u32_data;
        }
    }
    cgltf_free(data);

    scene->position_buffer = create_rw_buffer(total_position_data.data(), total_position_data.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    scene->normal_buffer = create_rw_buffer(total_normal_data.data(), total_normal_data.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    scene->uv_buffer = create_rw_buffer(total_uv_data.data(), total_uv_data.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    scene->index_buffer = create_rw_buffer(total_index_data.data(), total_index_data.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    scene->filtered_index_buffer = create_rw_buffer(nullptr, total_index_data.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    scene->mesh_constants_buffer = create_rw_buffer(mesh_constants_list.data(), mesh_constants_list.size() * sizeof(MeshConstants));
    scene->draw_command_buffer = create_rw_buffer(scene->draw_commands.data(), scene->draw_commands.size() * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    return scene;
}