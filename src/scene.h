#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <math/bounding_box.h>
#include <rhi/ez_vulkan.h>

#define CLUSTER_SIZE 256

struct ClusterCompact
{
    uint32_t triangle_count;
    uint32_t cluster_start;
};

struct Cluster
{
    glm::vec3 aabb_min, aabb_max;
    glm::vec3 cone_center, cone_axis;
    float cone_angle_cosine;
    float distance_from_camera;
    bool valid;
};

struct Mesh
{
    std::vector<ClusterCompact> compacts;
    std::vector<Cluster> clusters;
};

struct MeshConstants
{
    uint32_t face_count;
    uint32_t index_offset;
};

class Scene
{
public:
    ~Scene();
    std::vector<Mesh> meshs;
    std::vector<VkDrawIndexedIndirectCommand> draw_commands;
    EzBuffer position_buffer = VK_NULL_HANDLE;
    EzBuffer normal_buffer = VK_NULL_HANDLE;
    EzBuffer uv_buffer = VK_NULL_HANDLE;
    EzBuffer index_buffer = VK_NULL_HANDLE;
    EzBuffer filtered_index_buffer = VK_NULL_HANDLE;
    EzBuffer mesh_constants_buffer = VK_NULL_HANDLE;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
};