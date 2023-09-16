#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <math/bounding_box.h>
#include <rhi/ez_vulkan.h>

#define MESHLET_SIZE 256

struct Triangle
{
    glm::vec3 v[3];
};

struct MeshletCompact
{
    uint32_t triangle_count;
    uint32_t meshlet_start;
};

struct Meshlet
{
    glm::vec3 aabb_min, aabb_max;
    glm::vec3 cone_center, cone_axis;
    float cone_angle_cosine;
    float distance_from_camera;
    bool valid;
};

struct MeshletBatch
{
    std::vector<MeshletCompact> compacts;
    std::vector<Meshlet> meshlets;
};

class Scene
{
public:
    ~Scene();
    std::vector<MeshletBatch> meshlet_batchs;
    std::vector<VkDrawIndexedIndirectCommand> draw_args;
    EzBuffer position_buffer = VK_NULL_HANDLE;
    EzBuffer normal_buffer = VK_NULL_HANDLE;
    EzBuffer uv_buffer = VK_NULL_HANDLE;
    EzBuffer index_buffer = VK_NULL_HANDLE;
    EzBuffer transform_buffer = VK_NULL_HANDLE;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
};