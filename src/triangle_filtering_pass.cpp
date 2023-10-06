#include "triangle_filtering_pass.h"
#include "renderer.h"
#include "scene.h"
#include "camera.h"
#include <rhi/shader_manager.h>

TriangleFilteringPass::TriangleFilteringPass(Renderer* renderer)
{
    _renderer = renderer;

    _small_batch_chunk.batch_datas.resize(BATCH_COUNT);

    EzBufferDesc buffer_desc{};
    buffer_desc.size = sizeof(SmallBatchData) * BATCH_COUNT;
    buffer_desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_desc.memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ez_create_buffer(buffer_desc, _small_batch_buffer);

    buffer_desc.size = sizeof(UncompactedDrawCommand) * MAX_DRAW_CMD_COUNT;
    ez_create_buffer(buffer_desc, _uncompacted_draw_command_buffer);

    buffer_desc.size = sizeof(DrawCounter);
    ez_create_buffer(buffer_desc, _draw_counter_buffer);

    buffer_desc.size = sizeof(VkDrawIndexedIndirectCommand) * MAX_DRAW_CMD_COUNT;
    buffer_desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    ez_create_buffer(buffer_desc, _draw_command_buffer);
}

TriangleFilteringPass::~TriangleFilteringPass()
{
    ez_destroy_buffer(_small_batch_buffer);
    ez_destroy_buffer(_uncompacted_draw_command_buffer);
    ez_destroy_buffer(_draw_command_buffer);
    ez_destroy_buffer(_draw_counter_buffer);
}

void TriangleFilteringPass::render()
{
    ez_reset_pipeline_state();

    VkBufferMemoryBarrier2 barriers[3];
    barriers[0] = ez_buffer_barrier(_draw_counter_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    barriers[1] = ez_buffer_barrier(_draw_command_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    barriers[2] = ez_buffer_barrier(_uncompacted_draw_command_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    ez_pipeline_barrier(0, 3, barriers, 0, nullptr);

    // Clear buffers
    ez_bind_buffer(0, _draw_counter_buffer, _draw_counter_buffer->size);
    ez_bind_buffer(1, _uncompacted_draw_command_buffer, _uncompacted_draw_command_buffer->size);
    ez_set_compute_shader(ShaderManager::get()->get_shader("shader://clear_buffers.comp"));
    ez_dispatch(std::max(1u, (uint32_t)(MAX_DRAW_CMD_COUNT) / 256), 1, 1);

    // Synchronization
    barriers[0] = ez_buffer_barrier(_draw_counter_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    barriers[1] = ez_buffer_barrier(_draw_command_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    barriers[2] = ez_buffer_barrier(_uncompacted_draw_command_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    ez_pipeline_barrier(0, 3, barriers, 0, nullptr);

    // Triangle filtering
    int accum_draw_count = 0;
    int accum_num_triangles = 0;
    int accum_num_triangles_at_start_of_batch = 0;
    int batch_start = 0;
    _small_batch_chunk.current_batch_count = 0;
    _small_batch_chunk.current_draw_call_count = 0;

    for (int i = 0; i < _renderer->_scene->meshs.size(); ++i)
    {
        Mesh* mesh = &_renderer->_scene->meshs[i];
        for (int j = 0; j < mesh->clusters.size(); ++j)
        {
            const Cluster* cluster = &mesh->clusters[j];
            const ClusterCompact* compact = &mesh->compacts[j];

            bool cull_cluster = false;
            if (cluster->valid)
            {
                glm::vec3 test_vec = glm::normalize(_renderer->_camera->get_translation() - cluster->cone_center);
                if (glm::dot(test_vec, cluster->cone_axis) < cluster->cone_angle_cosine)
                    cull_cluster = true;
            }

            if (!cull_cluster)
            {
                SmallBatchData* batch_data = &(_small_batch_chunk.batch_datas[_small_batch_chunk.current_batch_count]);
                batch_data->accum_draw_index = accum_draw_count;
                batch_data->face_count = compact->triangle_count;
                batch_data->mesh_index = i;
                batch_data->index_offset = compact->cluster_start * 3;
                batch_data->output_index_offset = accum_num_triangles_at_start_of_batch * 3;
                batch_data->draw_batch_start = batch_start;

                _small_batch_chunk.current_batch_count++;
                accum_num_triangles += compact->triangle_count;
            }

            if (_small_batch_chunk.current_batch_count >= BATCH_COUNT)
            {
                accum_draw_count++;

                filter_triangles();

                batch_start = 0;
                accum_num_triangles_at_start_of_batch = accum_num_triangles;
            }
        }

        if (_small_batch_chunk.current_batch_count > 0)
        {
            accum_draw_count++;
            batch_start = _small_batch_chunk.current_batch_count;
            accum_num_triangles_at_start_of_batch = accum_num_triangles;
        }
    }

    filter_triangles();

    _draw_count = accum_draw_count;

    // Synchronization
    barriers[0] = ez_buffer_barrier(_uncompacted_draw_command_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    ez_pipeline_barrier(0, 1, barriers, 0, nullptr);

    // Batch compaction
    ez_bind_buffer(0, _draw_counter_buffer, _draw_counter_buffer->size);
    ez_bind_buffer(1, _uncompacted_draw_command_buffer, _uncompacted_draw_command_buffer->size);
    ez_bind_buffer(2, _draw_command_buffer, _draw_command_buffer->size);
    ez_set_compute_shader(ShaderManager::get()->get_shader("shader://batch_compaction.comp"));
    ez_dispatch(std::max(1u, (uint32_t)(MAX_DRAW_CMD_COUNT) / 256), 1, 1);
}

void TriangleFilteringPass::filter_triangles()
{
    VkBufferMemoryBarrier2 barrier = ez_buffer_barrier(_small_batch_buffer, EZ_RESOURCE_STATE_COPY_DEST);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);

    ez_update_buffer(_small_batch_buffer, _small_batch_chunk.current_batch_count * sizeof(SmallBatchData), 0, _small_batch_chunk.batch_datas.data());

    barrier = ez_buffer_barrier(_small_batch_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);

    ez_bind_buffer(0, _renderer->_scene->position_buffer, _renderer->_scene->position_buffer->size);
    ez_bind_buffer(1, _renderer->_scene->index_buffer, _renderer->_scene->index_buffer->size);
    ez_bind_buffer(2, _renderer->_scene->mesh_constants_buffer, _renderer->_scene->mesh_constants_buffer->size);
    ez_bind_buffer(3, _small_batch_buffer, _small_batch_buffer->size);
    ez_bind_buffer(4, _renderer->_scene->filtered_index_buffer, _renderer->_scene->filtered_index_buffer->size);
    ez_bind_buffer(5, _uncompacted_draw_command_buffer, _uncompacted_draw_command_buffer->size);
    ez_bind_buffer(6, _renderer->_view_buffer, _renderer->_view_buffer->size);
    ez_set_compute_shader(ShaderManager::get()->get_shader("shader://triangle_filtering.comp"));
    ez_dispatch(_small_batch_chunk.current_batch_count, 1, 1);

    _small_batch_chunk.current_batch_count = 0;
    _small_batch_chunk.current_draw_call_count = 0;
}

EzBuffer TriangleFilteringPass::get_index_buffer()
{
    return _renderer->_scene->filtered_index_buffer;
}

uint32_t TriangleFilteringPass::get_draw_count()
{
    return _draw_count;
}

EzBuffer TriangleFilteringPass::get_draw_command_buffer()
{
    return _draw_command_buffer;
}