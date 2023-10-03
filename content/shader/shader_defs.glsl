#ifndef SHADER_DEFS_H
#define SHADER_DEFS_H

#define MAX_DRAW_CMD_COUNT 256

struct MeshConstants
{
    uint face_count;
    uint index_offset;
};

struct SmallBatchData
{
    uint mesh_index;
    uint index_offset;
    uint face_count;
    uint output_index_offset;
    uint draw_batch_start;
    uint accum_draw_index;
};

struct UncompactedDrawCommand
{
    uint num_indices;
    uint start_index;
};

struct DrawIndexedIndirectCommand
{
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

#endif