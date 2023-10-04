#include "scene.h"

Scene::~Scene()
{
    ez_destroy_buffer(position_buffer);
    ez_destroy_buffer(normal_buffer);
    ez_destroy_buffer(uv_buffer);
    ez_destroy_buffer(index_buffer);
    ez_destroy_buffer(filtered_index_buffer);
    ez_destroy_buffer(mesh_constants_buffer);
}