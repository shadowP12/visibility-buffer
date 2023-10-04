#pragma once

#include <rhi/ez_vulkan.h>

#define BATCH_COUNT 512
#define MAX_DRAW_CMD_COUNT 256

class Renderer;

struct DrawCounter
{
    uint32_t count;
};

struct SmallBatchData
{
    uint32_t mesh_index;
    uint32_t index_offset;
    uint32_t face_count;
    uint32_t output_index_offset;
    uint32_t draw_batch_start;
    uint32_t accum_draw_index;
};

struct SmallBatchChunk
{
    uint32_t current_batch_count;
    uint32_t current_draw_call_count;
    std::vector<SmallBatchData> batch_datas;
};

struct UncompactedDrawCommand
{
    uint32_t num_indices;
    uint32_t start_index;
};

class TriangleFilteringPass
{
public:
    TriangleFilteringPass(Renderer* renderer);

    ~TriangleFilteringPass();

    void render();

    EzBuffer get_index_buffer();

    uint32_t get_draw_count();

    EzBuffer get_draw_command_buffer();

private:
    void filter_triangles();

    void batch_compaction();

private:
    Renderer* _renderer;
    uint32_t _draw_count = 0;
    SmallBatchChunk _small_batch_chunk;
    EzBuffer _small_batch_buffer = VK_NULL_HANDLE;
    EzBuffer _uncompacted_draw_command_buffer = VK_NULL_HANDLE;
    EzBuffer _draw_command_buffer = VK_NULL_HANDLE;
    EzBuffer _draw_counter_buffer = VK_NULL_HANDLE;
};