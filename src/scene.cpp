#include "scene.h"

Scene::~Scene()
{
    for (auto& prim : primitives)
    {
        ez_destroy_buffer(prim->index_buffer);
        ez_destroy_buffer(prim->vertex_buffer);
    }
    primitives.clear();
    meshes.clear();
    nodes.clear();
}